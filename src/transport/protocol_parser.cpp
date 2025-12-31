#include "protocol_parser.h"
#include <QDebug>

namespace transport {

ProtocolParser::ProtocolParser(QObject* parent)
    : QObject(parent)
    , m_state(ParserState::WaitingForCommand)
    , m_currentCommand(0)
    , m_expectedLength(0)
    , m_receivedLength(0)
    , m_inStructuredField(false)
{
}

void ProtocolParser::parseData(const QByteArray& data) {
    for (uint8_t byte : data) {
        processByte(byte);
    }
}

void ProtocolParser::reset() {
    m_state = ParserState::WaitingForCommand;
    m_buffer.clear();
    m_currentCommand = 0;
    m_expectedLength = 0;
    m_receivedLength = 0;
    m_inStructuredField = false;
}

void ProtocolParser::processByte(uint8_t byte) {
    switch (m_state) {
        case ParserState::WaitingForCommand:
            processCommand(byte);
            break;
            
        case ParserState::ReadingLength:
            m_buffer.append(byte);
            m_receivedLength++;
            
            if (m_receivedLength >= 2) {
                // Read length (2 bytes, big-endian)
                m_expectedLength = (static_cast<uint16_t>(m_buffer[0]) << 8) | 
                                   static_cast<uint16_t>(m_buffer[1]);
                
                if (m_expectedLength == 0) {
                    // Zero length, back to waiting for command
                    m_state = ParserState::WaitingForCommand;
                    m_buffer.clear();
                } else if (m_expectedLength < 2) {
                    // Invalid length
                    emit parseError("Invalid length in TN5250 command");
                    reset();
                } else {
                    // Start reading data (length includes the 2 length bytes)
                    m_state = ParserState::ReadingData;
                    m_receivedLength = 2; // Already have length bytes
                    m_buffer.clear();
                }
            }
            break;
            
        case ParserState::ReadingData:
            m_buffer.append(byte);
            m_receivedLength++;
            
            if (m_receivedLength >= m_expectedLength) {
                // Complete command received
                QByteArray commandData = m_buffer;
                
                // Check for structured field
                if (m_inStructuredField && commandData.size() >= 3) {
                    uint8_t type = static_cast<uint8_t>(commandData[2]);
                    StructuredFieldType sfType = static_cast<StructuredFieldType>(type);
                    QByteArray sfData = commandData.mid(3);
                    emit structuredFieldReceived(sfType, sfData);
                } else if (!m_inStructuredField) {
                    TN5250Command cmd = static_cast<TN5250Command>(m_currentCommand);
                    emit commandReceived(cmd, commandData);
                }
                
                // Reset for next command
                m_state = ParserState::WaitingForCommand;
                m_buffer.clear();
                m_inStructuredField = false;
            }
            break;
            
        case ParserState::Complete:
            // Should not reach here
            reset();
            break;
    }
}

void ProtocolParser::processCommand(uint8_t cmd) {
    m_currentCommand = cmd;
    
    // Check if it's a structured field command
    if (cmd == static_cast<uint8_t>(TN5250Command::WRITE_STRUCTURED_FIELD)) {
        m_inStructuredField = true;
    }
    
    // All TN5250 commands start with command byte followed by 2-byte length
    m_state = ParserState::ReadingLength;
    m_receivedLength = 0;
    m_expectedLength = 0;
    m_buffer.clear();
}

void ProtocolParser::processStructuredField(const QByteArray& data) {
    if (data.size() < 3) {
        emit parseError("Structured field too short");
        return;
    }
    
    uint8_t type = static_cast<uint8_t>(data[2]);
    StructuredFieldType sfType = static_cast<StructuredFieldType>(type);
    QByteArray sfData = data.mid(3);
    
    emit structuredFieldReceived(sfType, sfData);
}

} // namespace transport

