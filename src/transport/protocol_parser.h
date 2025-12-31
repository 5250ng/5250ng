#pragma once

#include <QByteArray>
#include <QObject>
#include <cstdint>

namespace transport {

// TN5250 command codes (based on RFC 1205 and IBM documentation)
enum class TN5250Command : uint8_t {
    WRITE_STRUCTURED_FIELD = 0x11,
    ERASE_WRITE            = 0x05,
    ERASE_WRITE_ALTERNATE  = 0x0D,
    READ_MODIFY            = 0x06,
    READ_MODIFY_WRITE      = 0x07
};

// TN5250 structured field types
enum class StructuredFieldType : uint8_t {
    OUTBOUND_3270DS        = 0x00,
    SCS                    = 0x01,
    OUTBOUND_5250_DS       = 0x02,
    INBOUND_3270DS         = 0x80,
    INBOUND_5250_DS        = 0x82
};

// Protocol parser state
enum class ParserState {
    WaitingForCommand,
    ReadingLength,
    ReadingData,
    Complete
};

class ProtocolParser : public QObject {
    Q_OBJECT

public:
    explicit ProtocolParser(QObject* parent = nullptr);
    
    // Parse incoming data stream
    void parseData(const QByteArray& data);
    
    // Reset parser state
    void reset();
    
    // Get current state
    ParserState state() const { return m_state; }

signals:
    void commandReceived(TN5250Command cmd, const QByteArray& data);
    void structuredFieldReceived(StructuredFieldType type, const QByteArray& data);
    void parseError(const QString& error);

private:
    void processByte(uint8_t byte);
    void processCommand(uint8_t cmd);
    void processStructuredField(const QByteArray& data);
    
    ParserState m_state;
    QByteArray m_buffer;
    uint8_t m_currentCommand;
    uint16_t m_expectedLength;
    uint16_t m_receivedLength;
    bool m_inStructuredField;
};

} // namespace transport

