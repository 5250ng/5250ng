#include "network/tn5250/client/decoder.h"
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
    Decoder *m_parser;
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
    void onParseError(const QString &error);
    void onRawScreenDataReceived(const QByteArray &data);
    void onClearScreenRequested();

  private:
    // Build a minimal valid GDS record with the given opcode and payload.
    // GDS layout (post-Telnet, GDS type = 0x12A0):
    //   [0-1] recLen big-endian (= body.size())
    //   [2-3] 0x12 0xA0
    //   [4-5] 0x00 0x00 reserved
    //   [6]   varLen = 4
    //   [7]   flagsHi = 0x00
    //   [8]   flagsLo = 0x00
    //   [9]   opcode
    //   [10+] payload
    static QByteArray makeGDS(uint8_t opcode, const QByteArray &payload);

    // Build the payload bytes for a Write-To-Display command (ESC 0x11 ctrl1 ctrl2 data).
    static QByteArray makeWTDPayload(const QByteArray &displayData);

    // Build the payload bytes for a Clear-Unit command (ESC 0x40).
    static QByteArray makeClearUnitPayload();
};

void TestDecoder::init() {
    m_parser = new Decoder(this);
    connect(m_parser, &Decoder::commandReceived, this, &TestDecoder::onCommandReceived);
    connect(m_parser, &Decoder::structuredFieldReceived,
            this, &TestDecoder::onStructuredFieldReceived);
    connect(m_parser, &Decoder::parseError, this, &TestDecoder::onParseError);
    connect(m_parser, &Decoder::rawScreenDataReceived,
            this, &TestDecoder::onRawScreenDataReceived);
    connect(m_parser, &Decoder::clearScreenRequested,
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

void TestDecoder::onParseError(const QString &error) { m_lastError = error; }

void TestDecoder::onRawScreenDataReceived(const QByteArray &data) {
    m_lastRawData = data;
}

void TestDecoder::onClearScreenRequested() { m_clearCount++; }

QByteArray TestDecoder::makeGDS(uint8_t opcode, const QByteArray &payload) {
    QByteArray body;
    body.append('\x12');                    // GDS type hi
    body.append('\xA0');                    // GDS type lo
    body.append('\x00');                    // reserved
    body.append('\x00');                    // reserved
    body.append('\x04');                    // varLen = 4
    body.append('\x00');                    // flagsHi
    body.append('\x00');                    // flagsLo
    body.append(static_cast<char>(opcode)); // opcode
    body.append(payload);

    // Record length includes the 2-byte length field itself
    uint16_t recLen = static_cast<uint16_t>(body.size() + 2);
    QByteArray rec;
    rec.append(static_cast<char>((recLen >> 8) & 0xFF));
    rec.append(static_cast<char>(recLen & 0xFF));
    rec.append(body);
    return rec;
}

QByteArray TestDecoder::makeWTDPayload(const QByteArray &displayData) {
    QByteArray p;
    p.append('\x04'); // ESC
    p.append('\x11'); // Write To Display CC
    p.append('\x00'); // ctrl1
    p.append('\x00'); // ctrl2 (bit3=0: no keyboard unlock)
    p.append(displayData);
    return p;
}

QByteArray TestDecoder::makeClearUnitPayload() {
    QByteArray p;
    p.append('\x04'); // ESC
    p.append('\x40'); // Clear Unit CC
    return p;
}

void TestDecoder::testReset() {
    // Feed a single byte (incomplete GDS), then reset
    m_parser->parseData(QByteArray::fromHex("05"));
    m_parser->reset();
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
}

void TestDecoder::testSimpleCommand() {
    // An incomplete GDS record (only 3 bytes) is silently buffered
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x02));

    m_parser->parseData(data);

    // Partial data buffers without error; state is still WaitingForCommand
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QVERIFY(m_lastError.isEmpty());
}

void TestDecoder::testCommandWithData() {
    // Build a GDS record: opcode 0x02 (Output Only), WTD with 5 display bytes.
    // Note: 0x01 is the SOH order byte and 0x04 is ESC — use EBCDIC data bytes instead.
    QByteArray displayData;
    displayData.append('\xC1'); // EBCDIC 'A'
    displayData.append('\xC2'); // EBCDIC 'B'
    displayData.append('\xC3'); // EBCDIC 'C'
    displayData.append('\xC4'); // EBCDIC 'D'
    displayData.append('\xC5'); // EBCDIC 'E'

    QByteArray gds = makeGDS(0x02, makeWTDPayload(displayData));
    m_parser->parseData(gds);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_lastRawData.size(), 5);
    QCOMPARE(static_cast<uint8_t>(m_lastRawData[0]), static_cast<uint8_t>(0xC1));
}

void TestDecoder::testStructuredField() {
    // Write Structured Field (CC=0x12) is not yet handled by the decoder.
    // Verify that a GDS record with unknown CC bytes doesn't crash.
    QByteArray sfPayload;
    sfPayload.append('\x04'); // ESC
    sfPayload.append('\x12'); // Write Structured Field CC (unhandled)
    sfPayload.append(static_cast<char>(StructuredFieldType::OUTBOUND_5250_DS));
    sfPayload.append('\xAA');
    sfPayload.append('\xBB');

    QByteArray gds = makeGDS(0x02, sfPayload);
    m_parser->parseData(gds);

    // Should not crash and should not emit an error for unrecognised CC
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QVERIFY(m_lastError.isEmpty());
}

void TestDecoder::testInvalidLength() {
    // Build a GDS record with varLen=2 (invalid — must be >= 4)
    QByteArray body;
    body.append('\x12'); // GDS type hi
    body.append('\xA0'); // GDS type lo
    body.append('\x00'); // reserved
    body.append('\x00'); // reserved
    body.append('\x02'); // varLen = 2 (INVALID)
    body.append('\x00');
    body.append('\x00');
    body.append('\x01');
    body.append('\x02');

    // Record length includes the 2-byte length field itself
    uint16_t recLen = static_cast<uint16_t>(body.size() + 2);
    QByteArray rec;
    rec.append(static_cast<char>((recLen >> 8) & 0xFF));
    rec.append(static_cast<char>(recLen & 0xFF));
    rec.append(body);

    m_parser->parseData(rec);

    QVERIFY(!m_lastError.isEmpty());
}

void TestDecoder::testStateTransitions() {
    // The GDS-based decoder is always in WaitingForCommand state.
    // Verify that partial records are buffered and processed once complete.
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);

    QByteArray gds = makeGDS(0x02, makeClearUnitPayload());

    // Send only the first 4 bytes — record is not yet complete
    m_parser->parseData(gds.left(4));
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_clearCount, 0); // not yet processed

    // Send the remainder — record is now complete and processed
    m_parser->parseData(gds.mid(4));
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_clearCount, 1); // ClearUnit was fired
}

void TestDecoder::testMultipleCommands() {
    // Two back-to-back GDS records: ClearUnit then WTD with display data
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
