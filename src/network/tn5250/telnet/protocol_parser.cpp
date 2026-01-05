#include "protocol_parser.h"
#include "core/logger.h"
#include <QDebug>

namespace telnet {

ProtocolParser::ProtocolParser(QObject *parent)
    : QObject(parent), m_state(ParserState::WaitingForCommand),
      m_currentCommand(0), m_expectedLength(0), m_receivedLength(0),
      m_inStructuredField(false) {}

void ProtocolParser::parseData(const QByteArray &data) {
    logger::Logger::instance()->debug(
        QString("ProtocolParser: parseData called with %1 bytes, state=%2, first "
                "bytes hex=%3")
            .arg(data.size())
            .arg(static_cast<int>(m_state))
            .arg(QString::fromLatin1(data.left(16).toHex())));

    // If we're waiting for a command and the first byte is not a valid command,
    // this might be raw screen data
    if (m_state == ParserState::WaitingForCommand && data.size() > 0) {
        uint8_t firstByte = static_cast<uint8_t>(data[0]);
        bool isValidCommand =
            firstByte ==
                static_cast<uint8_t>(TN5250Command::WRITE_STRUCTURED_FIELD) ||
            firstByte == static_cast<uint8_t>(TN5250Command::ERASE_WRITE) ||
            firstByte ==
                static_cast<uint8_t>(TN5250Command::ERASE_WRITE_ALTERNATE) ||
            firstByte == static_cast<uint8_t>(TN5250Command::READ_MODIFY) ||
            firstByte == static_cast<uint8_t>(TN5250Command::READ_MODIFY_WRITE);

        if (!isValidCommand) {
            // This looks like raw screen data, not a TN5250 command
            // Valid TN5250 commands are: 0x05, 0x06, 0x07, 0x0D, 0x11
            // If we receive non-TN5250 data, it's likely a negotiation issue
            logger::Logger::instance()->warning(
                QString(
                    "ProtocolParser: First byte 0x%1 is not a TN5250 command byte "
                    "(valid commands: 0x05, 0x06, 0x07, 0x0D, 0x11), "
                    "treating as raw screen data - this may indicate a negotiation "
                    "problem")
                    .arg(firstByte, 2, 16, QChar('0')));
            emit rawScreenDataReceived(data);
            return;
        }
    }

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
                // Length includes the 2 length bytes. Remaining bytes are the payload.
                const uint16_t payloadLength = m_expectedLength - 2;

                // Start reading payload bytes
                m_state = ParserState::ReadingData;
                m_receivedLength = 0;
                m_buffer.clear();

                // Special-case: empty payload. Emit immediately.
                if (payloadLength == 0) {
                    if (m_inStructuredField) {
                        // Structured field with no type/data is invalid, but keep behavior
                        // predictable and just emit empty data.
                        emit structuredFieldReceived(static_cast<StructuredFieldType>(0),
                                                     QByteArray());
                    } else {
                        emit commandReceived(static_cast<TN5250Command>(m_currentCommand),
                                             QByteArray());
                    }

                    // Reset for next command
                    m_state = ParserState::WaitingForCommand;
                    m_buffer.clear();
                    m_inStructuredField = false;
                    m_expectedLength = 0;
                    m_receivedLength = 0;
                }
            }
        }
        break;

    case ParserState::ReadingData:
        m_buffer.append(byte);
        m_receivedLength++;

        // m_receivedLength counts payload bytes; m_expectedLength includes the 2
        // length bytes, so compare against the payload length.
        if (m_receivedLength >= static_cast<uint16_t>(m_expectedLength - 2)) {
            // Complete command received
            // Note: m_buffer contains ONLY payload bytes.
            QByteArray commandData = m_buffer;

            // Debug logging
            logger::Logger::instance()->debug(
                QString("ProtocolParser: Complete command received, cmd=0x%1, "
                        "length=%2, data size=%3, hex=%4")
                    .arg(static_cast<int>(m_currentCommand), 2, 16, QChar('0'))
                    .arg(m_expectedLength)
                    .arg(commandData.size())
                    .arg(QString::fromLatin1(m_buffer.toHex())));

            // Check for structured field
            if (m_inStructuredField && commandData.size() >= 1) {
                uint8_t type = static_cast<uint8_t>(commandData[0]);
                StructuredFieldType sfType = static_cast<StructuredFieldType>(type);
                QByteArray sfData = commandData.mid(1);
                logger::Logger::instance()->debug(
                    QString("ProtocolParser: Structured field type=0x%1, data size=%2")
                        .arg(static_cast<int>(type), 2, 16, QChar('0'))
                        .arg(sfData.size()));
                emit structuredFieldReceived(sfType, sfData);
            } else if (!m_inStructuredField) {
                TN5250Command cmd = static_cast<TN5250Command>(m_currentCommand);
                logger::Logger::instance()->debug(
                    QString("ProtocolParser: Emitting commandReceived, cmd=0x%1, data "
                            "size=%2")
                        .arg(static_cast<int>(cmd), 2, 16, QChar('0'))
                        .arg(commandData.size()));
                emit commandReceived(cmd, commandData);
            }

            // Reset for next command
            m_state = ParserState::WaitingForCommand;
            m_buffer.clear();
            m_inStructuredField = false;
            m_expectedLength = 0;
            m_receivedLength = 0;
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

    logger::Logger::instance()->debug(
        QString("ProtocolParser: processCommand called with cmd=0x%1")
            .arg(cmd, 2, 16, QChar('0')));

    // Check if it's a valid TN5250 command
    bool isValidCommand = false;
    if (cmd == static_cast<uint8_t>(TN5250Command::WRITE_STRUCTURED_FIELD) ||
        cmd == static_cast<uint8_t>(TN5250Command::ERASE_WRITE) ||
        cmd == static_cast<uint8_t>(TN5250Command::ERASE_WRITE_ALTERNATE) ||
        cmd == static_cast<uint8_t>(TN5250Command::READ_MODIFY) ||
        cmd == static_cast<uint8_t>(TN5250Command::READ_MODIFY_WRITE)) {
        isValidCommand = true;
    }

    if (!isValidCommand) {
        logger::Logger::instance()->warning(
            QString("ProtocolParser: Invalid command byte 0x%1, resetting parser")
                .arg(cmd, 2, 16, QChar('0')));
        reset();
        return;
    }

    // Check if it's a structured field command
    if (cmd == static_cast<uint8_t>(TN5250Command::WRITE_STRUCTURED_FIELD)) {
        m_inStructuredField = true;
        logger::Logger::instance()->debug(
            "ProtocolParser: Detected WRITE_STRUCTURED_FIELD");
    }

    // All TN5250 commands start with command byte followed by 2-byte length
    m_state = ParserState::ReadingLength;
    m_receivedLength = 0;
    m_expectedLength = 0;
    m_buffer.clear();
}

void ProtocolParser::processStructuredField(const QByteArray &data) {
    if (data.size() < 3) {
        emit parseError("Structured field too short");
        return;
    }

    uint8_t type = static_cast<uint8_t>(data[2]);
    StructuredFieldType sfType = static_cast<StructuredFieldType>(type);
    QByteArray sfData = data.mid(3);

    emit structuredFieldReceived(sfType, sfData);
}

} // namespace telnet
