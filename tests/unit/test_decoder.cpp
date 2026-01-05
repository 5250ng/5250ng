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

  private slots:
    void onCommandReceived(TN5250Command cmd, const QByteArray &data);
    void onStructuredFieldReceived(StructuredFieldType type, const QByteArray &data);
    void onParseError(const QString &error);
};

void TestDecoder::init() {
    m_parser = new Decoder(this);
    connect(m_parser, &Decoder::commandReceived,
            this, &TestDecoder::onCommandReceived);
    connect(m_parser, &Decoder::structuredFieldReceived,
            this, &TestDecoder::onStructuredFieldReceived);
    connect(m_parser, &Decoder::parseError,
            this, &TestDecoder::onParseError);

    m_lastCommand = static_cast<TN5250Command>(0);
    m_lastData.clear();
    m_lastSFType = static_cast<StructuredFieldType>(0);
    m_lastSFData.clear();
    m_lastError.clear();
}

void TestDecoder::cleanup() {
    m_parser->deleteLater();
    m_parser = nullptr;
}

void TestDecoder::onCommandReceived(TN5250Command cmd, const QByteArray &data) {
    m_lastCommand = cmd;
    m_lastData = data;
}

void TestDecoder::onStructuredFieldReceived(StructuredFieldType type, const QByteArray &data) {
    m_lastSFType = type;
    m_lastSFData = data;
}

void TestDecoder::onParseError(const QString &error) {
    m_lastError = error;
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
}

void TestDecoder::testCommandWithData() {
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x07)); // length
    data.append(static_cast<uint8_t>(0x01));
    data.append(static_cast<uint8_t>(0x02));
    data.append(static_cast<uint8_t>(0x03));
    data.append(static_cast<uint8_t>(0x04));
    data.append(static_cast<uint8_t>(0x05));

    m_parser->parseData(data);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_lastCommand, TN5250Command::ERASE_WRITE);
    QCOMPARE(m_lastData.size(), 5);
    QCOMPARE(m_lastData[0], static_cast<uint8_t>(0x01));
}

void TestDecoder::testStructuredField() {
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::WRITE_STRUCTURED_FIELD));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x06));
    data.append(static_cast<uint8_t>(StructuredFieldType::OUTBOUND_5250_DS));
    data.append(static_cast<uint8_t>(0xAA));
    data.append(static_cast<uint8_t>(0xBB));

    m_parser->parseData(data);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_lastSFType, StructuredFieldType::OUTBOUND_5250_DS);
    QCOMPARE(m_lastSFData.size(), 2);
}

void TestDecoder::testInvalidLength() {
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x01)); // Invalid

    m_parser->parseData(data);

    QVERIFY(!m_lastError.isEmpty());
}

void TestDecoder::testStateTransitions() {
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);

    QByteArray cmd;
    cmd.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    m_parser->parseData(cmd);
    QCOMPARE(m_parser->state(), ParserState::ReadingLength);

    QByteArray len1;
    len1.append(static_cast<uint8_t>(0x00));
    m_parser->parseData(len1);
    QCOMPARE(m_parser->state(), ParserState::ReadingLength);

    QByteArray len2;
    len2.append(static_cast<uint8_t>(0x05));
    m_parser->parseData(len2);
    QCOMPARE(m_parser->state(), ParserState::ReadingData);

    QByteArray data;
    data.append(static_cast<uint8_t>(0x01));
    data.append(static_cast<uint8_t>(0x02));
    data.append(static_cast<uint8_t>(0x03));
    m_parser->parseData(data);
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
}

void TestDecoder::testMultipleCommands() {
    QByteArray cmd1;
    cmd1.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    cmd1.append(static_cast<uint8_t>(0x00));
    cmd1.append(static_cast<uint8_t>(0x04));
    cmd1.append(static_cast<uint8_t>(0xAA));
    cmd1.append(static_cast<uint8_t>(0xBB));

    QByteArray cmd2;
    cmd2.append(static_cast<uint8_t>(TN5250Command::READ_MODIFY));
    cmd2.append(static_cast<uint8_t>(0x00));
    cmd2.append(static_cast<uint8_t>(0x04));
    cmd2.append(static_cast<uint8_t>(0xCC));
    cmd2.append(static_cast<uint8_t>(0xDD));

    QByteArray allData = cmd1 + cmd2;
    m_parser->parseData(allData);

    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_lastCommand, TN5250Command::READ_MODIFY);
    QCOMPARE(m_lastData.size(), 2);
    QCOMPARE(m_lastData[0], static_cast<uint8_t>(0xCC));
}

QTEST_MAIN(TestDecoder)
#include "test_decoder.moc"
