#include <QtTest/QtTest>
#include "transport/protocol_parser.h"

using namespace transport;

class TestProtocolParser : public QObject {
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
    ProtocolParser* m_parser;
    TN5250Command m_lastCommand;
    QByteArray m_lastData;
    StructuredFieldType m_lastSFType;
    QByteArray m_lastSFData;
    QString m_lastError;

private slots:
    void onCommandReceived(TN5250Command cmd, const QByteArray& data);
    void onStructuredFieldReceived(StructuredFieldType type, const QByteArray& data);
    void onParseError(const QString& error);
};

void TestProtocolParser::init() {
    m_parser = new ProtocolParser(this);
    connect(m_parser, &ProtocolParser::commandReceived,
            this, &TestProtocolParser::onCommandReceived);
    connect(m_parser, &ProtocolParser::structuredFieldReceived,
            this, &TestProtocolParser::onStructuredFieldReceived);
    connect(m_parser, &ProtocolParser::parseError,
            this, &TestProtocolParser::onParseError);
    
    m_lastCommand = static_cast<TN5250Command>(0);
    m_lastData.clear();
    m_lastSFType = static_cast<StructuredFieldType>(0);
    m_lastSFData.clear();
    m_lastError.clear();
}

void TestProtocolParser::cleanup() {
    m_parser->deleteLater();
    m_parser = nullptr;
}

void TestProtocolParser::onCommandReceived(TN5250Command cmd, const QByteArray& data) {
    m_lastCommand = cmd;
    m_lastData = data;
}

void TestProtocolParser::onStructuredFieldReceived(StructuredFieldType type, const QByteArray& data) {
    m_lastSFType = type;
    m_lastSFData = data;
}

void TestProtocolParser::onParseError(const QString& error) {
    m_lastError = error;
}

void TestProtocolParser::testReset() {
    m_parser->parseData(QByteArray::fromHex("05"));
    m_parser->reset();
    
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
}

void TestProtocolParser::testSimpleCommand() {
    // ERASE_WRITE command (0x05) with length 0x0002 (just length bytes)
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x02));
    
    m_parser->parseData(data);
    
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
}

void TestProtocolParser::testCommandWithData() {
    // ERASE_WRITE command with 5 bytes of data (length = 7: 2 length bytes + 5 data bytes)
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

void TestProtocolParser::testStructuredField() {
    // WRITE_STRUCTURED_FIELD command
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::WRITE_STRUCTURED_FIELD));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x06)); // length: 2 length + 1 type + 2 data = 5
    data.append(static_cast<uint8_t>(StructuredFieldType::OUTBOUND_5250_DS));
    data.append(static_cast<uint8_t>(0xAA));
    data.append(static_cast<uint8_t>(0xBB));
    
    m_parser->parseData(data);
    
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    QCOMPARE(m_lastSFType, StructuredFieldType::OUTBOUND_5250_DS);
    QCOMPARE(m_lastSFData.size(), 2);
}

void TestProtocolParser::testInvalidLength() {
    // Command with invalid length (1 byte, which is less than minimum 2)
    QByteArray data;
    data.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    data.append(static_cast<uint8_t>(0x00));
    data.append(static_cast<uint8_t>(0x01)); // Invalid: less than 2
    
    m_parser->parseData(data);
    
    QVERIFY(!m_lastError.isEmpty());
}

void TestProtocolParser::testStateTransitions() {
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
    
    // Send command byte
    QByteArray cmd;
    cmd.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    m_parser->parseData(cmd);
    QCOMPARE(m_parser->state(), ParserState::ReadingLength);
    
    // Send first length byte
    QByteArray len1;
    len1.append(static_cast<uint8_t>(0x00));
    m_parser->parseData(len1);
    QCOMPARE(m_parser->state(), ParserState::ReadingLength);
    
    // Send second length byte
    QByteArray len2;
    len2.append(static_cast<uint8_t>(0x05));
    m_parser->parseData(len2);
    QCOMPARE(m_parser->state(), ParserState::ReadingData);
    
    // Send data bytes
    QByteArray data;
    data.append(static_cast<uint8_t>(0x01));
    data.append(static_cast<uint8_t>(0x02));
    data.append(static_cast<uint8_t>(0x03));
    m_parser->parseData(data);
    QCOMPARE(m_parser->state(), ParserState::WaitingForCommand);
}

void TestProtocolParser::testMultipleCommands() {
    // First command
    QByteArray cmd1;
    cmd1.append(static_cast<uint8_t>(TN5250Command::ERASE_WRITE));
    cmd1.append(static_cast<uint8_t>(0x00));
    cmd1.append(static_cast<uint8_t>(0x04));
    cmd1.append(static_cast<uint8_t>(0xAA));
    cmd1.append(static_cast<uint8_t>(0xBB));
    
    // Second command
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

QTEST_MAIN(TestProtocolParser)
#include "test_protocol_parser.moc"

