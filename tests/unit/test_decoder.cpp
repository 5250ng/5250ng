// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "network/tn5250_qt/client/decoder_adapter.h"
#include <QtTest/QtTest>

using namespace tn5250::client;

class TestDecoder : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testReset();
    void testSimpleCommand();
    void testCommandWithData();
    void testStructuredField();
    void testInvalidLength();
    void testStateTransitions();
    void testMultipleCommands();

  private:
    DecoderAdapter *m_parser;
    TN5250Command m_lastCommand;
    QByteArray m_lastData;
    StructuredFieldType m_lastSFType;
    QByteArray m_lastSFData;
    QString m_lastError;
    QByteArray m_lastRawData;
    int m_clearCount;

  private slots:
    void onCommandReceived(TN5250Command cmd, const QByteArray &data);
    void onStructuredFieldReceived(StructuredFieldType type, const QByteArray &data);
    void onWriteStructuredFieldReceived(const QByteArray &data);
    void onParseError(const QString &error);
    void onRawScreenDataReceived(const QByteArray &data);
    void onClearScreenRequested();

  private:
    static QByteArray makeGDS(uint8_t opcode, const QByteArray &payload);
    static QByteArray makeWTDPayload(const QByteArray &displayData);
    static QByteArray makeClearUnitPayload();
};

void TestDecoder::init() {
    m_parser = new DecoderAdapter(this);
    connect(m_parser, &DecoderAdapter::commandReceived, this, &TestDecoder::onCommandReceived);
    connect(m_parser, &DecoderAdapter::structuredFieldReceived,
            this, &TestDecoder::onStructuredFieldReceived);
    connect(m_parser, &DecoderAdapter::writeStructuredFieldReceived,
            this, &TestDecoder::onWriteStructuredFieldReceived);
    connect(m_parser, &DecoderAdapter::parseError, this, &TestDecoder::onParseError);
    connect(m_parser, &DecoderAdapter::rawScreenDataReceived,
            this, &TestDecoder::onRawScreenDataReceived);
    connect(m_parser, &DecoderAdapter::clearScreenRequested,
            this, &TestDecoder::onClearScreenRequested);

    m_lastCommand = static_cast<TN5250Command>(0);
    m_lastData.clear();
    m_lastSFType = static_cast<StructuredFieldType>(0);
    m_lastSFData.clear();
    m_lastError.clear();
    m_lastRawData.clear();
    m_clearCount = 0;
}

void TestDecoder::cleanup() {
    m_parser->deleteLater();
    m_parser = nullptr;
}

void TestDecoder::onCommandReceived(TN5250Command cmd, const QByteArray &data) {
    m_lastCommand = cmd;
    m_lastData = data;
}

void TestDecoder::onStructuredFieldReceived(StructuredFieldType type,
                                            const QByteArray &data) {
    m_lastSFType = type;
    m_lastSFData = data;
}

void TestDecoder::onWriteStructuredFieldReceived(const QByteArray &data) {
    m_lastSFData = data;
}

void TestDecoder::onParseError(const QString &error) { m_lastError = error; }

void TestDecoder::onRawScreenDataReceived(const QByteArray &data) {
    m_lastRawData = data;
}

void TestDecoder::onClearScreenRequested() { m_clearCount++; }

QByteArray TestDecoder::makeGDS(uint8_t opcode, const QByteArray &payload) {
    QByteArray body;
    body.append('\x12');
    body.append('\xA0');
    body.append('\x00');
    body.append('\x00');
    body.append('\x04');
    body.append('\x00');
    body.append('\x00');
    body.append(static_cast<char>(opcode));
    body.append(payload);

    uint16_t recLen = static_cast<uint16_t>(body.size() + 2);
    QByteArray rec;
    rec.append(static_cast<char>((recLen >> 8) & 0xFF));
    rec.append(static_cast<char>(recLen & 0xFF));
    rec.append(body);
    return rec;
}

QByteArray TestDecoder::makeWTDPayload(const QByteArray &displayData) {
    QByteArray p;
    p.append('\x04');
    p.append('\x11');
    p.append('\x00');
    p.append('\x00');
    p.append(displayData);
    return p;
}

QByteArray TestDecoder::makeClearUnitPayload() {
    QByteArray p;
    p.append('\x04');
    p.append('\x40');
    return p;
}

void TestDecoder::testReset() {
    m_parser->parseData(QByteArray::fromHex("05"));
    m_parser->reset();
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
}

void TestDecoder::testSimpleCommand() {
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x02));

    m_parser->parseData(data);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QVERIFY(m_lastError.isEmpty());
}

void TestDecoder::testCommandWithData() {
    QByteArray displayData;
    displayData.append('\xC1');
    displayData.append('\xC2');
    displayData.append('\xC3');
    displayData.append('\xC4');
    displayData.append('\xC5');

    QByteArray gds = makeGDS(0x02, makeWTDPayload(displayData));
    m_parser->parseData(gds);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_lastRawData.size(), 5);
    QCOMPARE(static_cast<uint8_t>(m_lastRawData[0]), static_cast<uint8_t>(0xC1));
}

void TestDecoder::testStructuredField() {
    // Build a real Write Structured Field per RFC 1205 §12.5.1:
    //   ESC F3 [LL LL] [class] [type] [data]
    // where LL LL is the big-endian total SF length (length + class + type
    // + data). Length 6 = 2 (length) + 2 (class/type) + 2 (data).
    QByteArray sfPayload;
    sfPayload.append('\x04');
    sfPayload.append('\xF3');
    sfPayload.append('\x00');
    sfPayload.append('\x06');
    sfPayload.append('\xD9');
    sfPayload.append('\x70');
    sfPayload.append('\xAA');
    sfPayload.append('\xBB');

    QByteArray gds = makeGDS(0x02, sfPayload);
    m_parser->parseData(gds);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QVERIFY(m_lastError.isEmpty());
    QCOMPARE(m_lastSFData.size(), 6);
    QCOMPARE(static_cast<uint8_t>(m_lastSFData[0]), static_cast<uint8_t>(0x00));
    QCOMPARE(static_cast<uint8_t>(m_lastSFData[1]), static_cast<uint8_t>(0x06));
    QCOMPARE(static_cast<uint8_t>(m_lastSFData[2]), static_cast<uint8_t>(0xD9));
    QCOMPARE(static_cast<uint8_t>(m_lastSFData[3]), static_cast<uint8_t>(0x70));
    QCOMPARE(static_cast<uint8_t>(m_lastSFData[4]), static_cast<uint8_t>(0xAA));
    QCOMPARE(static_cast<uint8_t>(m_lastSFData[5]), static_cast<uint8_t>(0xBB));
}

void TestDecoder::testInvalidLength() {
    QByteArray body;
    body.append('\x12');
    body.append('\xA0');
    body.append('\x00');
    body.append('\x00');
    body.append('\x02'); // varLen = 2 (INVALID)
    body.append('\x00');
    body.append('\x00');
    body.append('\x01');
    body.append('\x02');

    uint16_t recLen = static_cast<uint16_t>(body.size() + 2);
    QByteArray rec;
    rec.append(static_cast<char>((recLen >> 8) & 0xFF));
    rec.append(static_cast<char>(recLen & 0xFF));
    rec.append(body);

    m_parser->parseData(rec);

    QVERIFY(!m_lastError.isEmpty());
}

void TestDecoder::testStateTransitions() {
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);

    QByteArray gds = makeGDS(0x02, makeClearUnitPayload());

    m_parser->parseData(gds.left(4));
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_clearCount, 0);

    m_parser->parseData(gds.mid(4));
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_clearCount, 1);
}

void TestDecoder::testMultipleCommands() {
    QByteArray displayData;
    displayData.append('\xCC');
    displayData.append('\xDD');

    QByteArray allData;
    allData += makeGDS(0x02, makeClearUnitPayload());
    allData += makeGDS(0x02, makeWTDPayload(displayData));

    m_parser->parseData(allData);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_clearCount, 1);
    QCOMPARE(m_lastRawData.size(), 2);
    QCOMPARE(static_cast<uint8_t>(m_lastRawData[0]), static_cast<uint8_t>(0xCC));
}

QTEST_MAIN(TestDecoder)
#include "test_decoder.moc"
