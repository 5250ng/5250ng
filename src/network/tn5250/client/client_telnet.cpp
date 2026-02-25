#include "../telnet/commands.h"
#include "../telnet/options.h"
#include "client.h"
#include "logger/logger.h"
#include <QChar>

namespace tn5250::client {

using telnet::TelnetCommand;
using telnet::telnetCommandToString;
using telnet::TelnetOption;
using telnet::telnetOptionToString;

void TN5250Client::processTelnetData(const QByteArray &data) {
    m_receiveBuffer.append(data);

    QByteArray processed;
    int i = 0;

    while (i < m_receiveBuffer.size()) {
        uint8_t byte = static_cast<uint8_t>(m_receiveBuffer[i]);

        if (byte == static_cast<uint8_t>(TelnetCommand::IAC)) {
            if (i + 1 >= m_receiveBuffer.size()) {
                // Need more data
                break;
            }

            uint8_t next = static_cast<uint8_t>(m_receiveBuffer[i + 1]);

            // Double IAC means literal IAC
            if (next == static_cast<uint8_t>(TelnetCommand::IAC)) {
                processed.append(static_cast<uint8_t>(TelnetCommand::IAC));
                i += 2;
                continue;
            }

            // Check if it's a standalone command (no option byte)
            if (telnet::isStandaloneTelnetCommand(next)) {
                // Debug logging for standalone commands
                TelnetCommand standaloneCmd = static_cast<TelnetCommand>(next);
                logger::Logger::instance()->debug(
                    QString("[TN5250->Client]: Received Telnet standalone command: IAC %1 "
                            "(0xFF 0x%2)")
                        .arg(telnetCommandToString(standaloneCmd))
                        .arg(next, 2, 16, QChar('0'))
                );

                // Standalone command - handle it
                if (next == static_cast<uint8_t>(TelnetCommand::SE)) {
                    // SE can end a subnegotiation
                    if (m_inSubnegotiation) {
                        handleSubnegotiation(m_currentSubnegotiation, m_subnegotiationBuffer);
                        m_subnegotiationBuffer.clear();
                        m_inSubnegotiation = false;
                    }
                } else if (next == static_cast<uint8_t>(TelnetCommand::EOR)) {
                    // EOR marks end of a TN5250 GDS record — strip it silently.
                    // The decoder handles record boundaries via the 2-byte length prefix.
                    logger::Logger::instance()->debug(
                        "[TN5250->Client]: Received IAC EOR (end of GDS record)"
                    );
                } else {
                    // Other standalone commands - just skip for now
                    logger::Logger::instance()->warning(
                        QString("[TN5250->Client]: Unknown or unhandled Telnet standalone "
                                "command 0x%1")
                            .arg(QString::number(next, 16).toUpper())
                    );
                }
                i += 2;
                continue;
            }

            // Command with option byte (WILL, WONT, DO, DONT, SB)
            if (i + 2 >= m_receiveBuffer.size()) {
                // Need more data
                break;
            }

            uint8_t cmd = next;
            uint8_t opt = static_cast<uint8_t>(m_receiveBuffer[i + 2]);

            handleTelnetCommand(cmd, opt);
            i += 3;
        } else if (m_inSubnegotiation) {
            m_subnegotiationBuffer.append(byte);
            i++;
        } else {
            processed.append(byte);
            i++;
        }
    }

    // Remove processed data from buffer
    if (i > 0) {
        m_receiveBuffer.remove(0, i);
    }

    // Process application data
    if (!processed.isEmpty()) {
        if (m_handshakeComplete) {
            logger::Logger::instance()->debug(
                QString("[TN5250->Client]: Received %1 bytes of application data, first "
                        "bytes hex=%2")
                    .arg(processed.size())
                    .arg(QString::fromLatin1(processed.left(32).toHex()))
            );
            emit dataReceived(processed);
        } else {
            logger::Logger::instance()->debug(
                QString("[TN5250->Client]: Received %1 bytes during handshake, first "
                        "bytes hex=%2")
                    .arg(processed.size())
                    .arg(QString::fromLatin1(processed.left(32).toHex()))
            );
            m_handshakeBuffer.append(processed);
            processHandshakeData(processed);
        }
    }
}

void TN5250Client::sendTelnetCommand(TelnetCommand cmd, TelnetOption opt) {
    if (!m_socket) {
        return;
    }

    QByteArray command;
    command.append(static_cast<uint8_t>(TelnetCommand::IAC));
    command.append(static_cast<uint8_t>(cmd));
    command.append(static_cast<uint8_t>(opt));

    m_socket->write(command);

    // Debug logging
    logger::Logger::instance()->debug(
        QString("[TN5250->Client]: Sent Telnet command: IAC %1 %2 (0xFF 0x%3 0x%4)")
            .arg(telnetCommandToString(cmd))
            .arg(telnetOptionToString(opt))
            .arg(static_cast<uint8_t>(cmd), 2, 16, QChar('0'))
            .arg(static_cast<uint8_t>(opt), 2, 16, QChar('0'))
    );
}

void TN5250Client::handleTelnetCommand(uint8_t cmd, uint8_t opt) {
    TelnetCommand command = static_cast<TelnetCommand>(cmd);
    TelnetOption option = static_cast<TelnetOption>(opt);

    // Debug logging
    logger::Logger::instance()->debug(
        QString(
            "[TN5250->Client]: Received Telnet command: IAC %1 %2 (0xFF 0x%3 0x%4)"
        )
            .arg(telnetCommandToString(command))
            .arg(telnetOptionToString(option))
            .arg(cmd, 2, 16, QChar('0'))
            .arg(opt, 2, 16, QChar('0'))
    );

    switch (command) {
    case TelnetCommand::WILL:
        // Server wants to enable an option
        if (option == TelnetOption::TRANSMIT_BINARY) {
            // Server is responding to our DO BINARY or offering binary mode
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::TRANSMIT_BINARY);
            m_binaryNegotiated = true;
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Binary mode negotiated (server WILL)"
            );
            checkHandshakeComplete();
        } else if (option == TelnetOption::END_OF_RECORD) {
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::END_OF_RECORD);
            m_eorNegotiated = true;
            logger::Logger::instance()->debug("[TN5250->Client]: EOR negotiated");
            checkHandshakeComplete();
        } else if (option == TelnetOption::TELNET_START_TLS) {
            handleStartTLS();
        } else {
            sendTelnetCommand(TelnetCommand::DONT, option);
        }
        break;

    case TelnetCommand::WONT:
        // Server refuses an option
        if (option == TelnetOption::TRANSMIT_BINARY ||
            option == TelnetOption::END_OF_RECORD) {
            logger::Logger::instance()->warning(
                QString("[TN5250->Client]: Server refused %1 option")
                    .arg(option == TelnetOption::TRANSMIT_BINARY ? "BINARY" : "EOR")
            );
        }
        break;

    case TelnetCommand::DO:
        // Server wants us to enable an option
        if (option == TelnetOption::TRANSMIT_BINARY) {
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::TRANSMIT_BINARY);
            m_binaryNegotiated = true;
            logger::Logger::instance()->debug("[TN5250->Client]: Binary mode enabled");
            checkHandshakeComplete();
        } else if (option == TelnetOption::END_OF_RECORD) {
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::END_OF_RECORD);
            m_eorNegotiated = true;
            logger::Logger::instance()->debug("[TN5250->Client]: EOR enabled");
            checkHandshakeComplete();
        } else if (option == TelnetOption::TERMINAL_TYPE) {
            // Accept TERMINAL_TYPE so server can request our terminal type
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::TERMINAL_TYPE);
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Accepted TERMINAL_TYPE negotiation"
            );
        } else if (option == TelnetOption::NEW_ENVIRON) {
            // Accept NEW_ENVIRON so server can request environment variables
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::NEW_ENVIRON);
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Accepted NEW_ENVIRON negotiation"
            );
        } else {
            sendTelnetCommand(TelnetCommand::WONT, option);
        }
        break;

    case TelnetCommand::DONT:
        // Server wants us to disable an option
        break;

    case TelnetCommand::SB:
        // Start subnegotiation
        m_inSubnegotiation = true;
        m_currentSubnegotiation = option;
        m_subnegotiationBuffer.clear();
        break;

    default:
        logger::Logger::instance()->debug(
            QString("[TN5250->Client]: Unhandled telnet command: %1").arg(cmd)
        );
        break;
    }
}

void TN5250Client::handleSubnegotiation(TelnetOption opt, const QByteArray &data) {
    switch (opt) {
    case TelnetOption::TERMINAL_TYPE:
        // Handle terminal type subnegotiation
        // Server sends: IAC SB TERMINAL_TYPE SEND IAC SE
        // We respond: IAC SB TERMINAL_TYPE IS <name> IAC SE
        // Some servers send empty subnegotiation (just SB TERMINAL_TYPE SE)
        // which also means "send your terminal type"
        if (data.size() >= 1 && data[0] == 1) { // SEND = 1
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Terminal type requested by server (SEND)"
            );
            sendDeviceName();
        } else if (data.isEmpty()) {
            // Empty subnegotiation also means "send your terminal type"
            logger::Logger::instance()->debug("[TN5250->Client]: Terminal type requested "
                                              "by server (empty subnegotiation)");
            sendDeviceName();
        } else {
            logger::Logger::instance()->debug(
                QString("[TN5250->Client]: Terminal type subnegotiation (data: %1)")
                    .arg(QString::fromLatin1(data.toHex()))
            );
            // Still respond with terminal type even if format is unexpected
            sendDeviceName();
        }
        break;

    case TelnetOption::NEW_ENVIRON:
        // Handle NEW_ENVIRON subnegotiation
        // Server may send:
        // - IAC SB NEW_ENVIRON SEND (0x01) IAC SE - request to send
        // - IAC SB NEW_ENVIRON IS (0x00) ... - server sending environment
        // - IAC SB NEW_ENVIRON INFO (0x02) ... - server sending environment (alt)
        // We always respond with: IAC SB NEW_ENVIRON IS VAR KBDTYPE USERVAR BRB
        // VAR CODEPAGE USERVAR 37 VAR CHARSET USERVAR 697 IAC SE
        if (data.size() >= 1) {
            uint8_t firstByte = static_cast<uint8_t>(data[0]);
            if (firstByte == 0x01) { // SEND = 0x01 (request to send)
                logger::Logger::instance()->debug(
                    "[TN5250->Client]: NEW_ENVIRON requested by server (SEND)"
                );
            } else if (firstByte == 0x00) { // IS = 0x00 (server sending)
                logger::Logger::instance()->debug(
                    QString("[TN5250->Client]: NEW_ENVIRON IS from server (data: %1)")
                        .arg(QString::fromLatin1(data.mid(1).toHex()))
                );
            } else if (firstByte == 0x02) { // INFO = 0x02 (server sending, alt)
                logger::Logger::instance()->debug(
                    QString("[TN5250->Client]: NEW_ENVIRON INFO from server (data: %1)")
                        .arg(QString::fromLatin1(data.mid(1).toHex()))
                );
            } else {
                logger::Logger::instance()->debug(
                    QString(
                        "[TN5250->Client]: NEW_ENVIRON subnegotiation (first byte=0x%1, "
                        "data: %2)"
                    )
                        .arg(firstByte, 2, 16, QChar('0'))
                        .arg(QString::fromLatin1(data.toHex()))
                );
            }
        } else if (data.isEmpty()) {
            // Empty subnegotiation also means "send your environment"
            logger::Logger::instance()->debug("[TN5250->Client]: NEW_ENVIRON requested "
                                              "by server (empty subnegotiation)");
        }
        // Always respond with our environment variables
        sendNewEnviron();
        break;

    case TelnetOption::NEGOTIATE_ABOUT_WINDOW_SIZE:
        // Handle window size subnegotiation
        logger::Logger::instance()->debug("[TN5250->Client]: NAWS subnegotiation");
        break;

    default:
        logger::Logger::instance()->debug(
            QString("[TN5250->Client]: Unhandled subnegotiation for option: %1")
                .arg(static_cast<int>(opt))
        );
        break;
    }
}

void TN5250Client::sendData(const QByteArray &data) {
    if (!isConnected()) {
        logger::Logger::instance()->warning(
            "[TN5250->Client]: Cannot send data, not connected"
        );
        return;
    }

    // Wrap payload in a GDS record per RFC 1205:
    // [recLen(2)] [0x12A0(2)] [0x0000(2)] [varLen=0x04(1)] [flagsHi(1)] [flagsLo(1)] [opcode(1)] [payload]
    // Then IAC-escape the entire record and append IAC EOR.
    QByteArray record;
    int varHdrLen = 4; // flags(2) + opcode(1) + varLen byte itself counted separately
    int totalLen = 4 + 1 + varHdrLen + data.size(); // recType(2) + reserved(2) + varLen(1) + varHdr(4) + payload
    // recLen = total bytes after the 2-byte length field
    int recLen = totalLen;

    // 2-byte big-endian record length
    record.append(static_cast<char>((recLen >> 8) & 0xFF));
    record.append(static_cast<char>(recLen & 0xFF));
    // Record type 0x12A0
    record.append(static_cast<char>(0x12));
    record.append(static_cast<char>(0xA0));
    // Reserved 0x0000
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(0x00));
    // Variable header length
    record.append(static_cast<char>(varHdrLen));
    // Flags (0x0000) and opcode (0x00 = no-op / general response)
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(0x00));
    // Payload
    record.append(data);

    sendRawData(record);
}

void TN5250Client::sendRawData(const QByteArray &data) {
    if (!m_socket) {
        return;
    }

    // Escape IAC bytes in data
    QByteArray escaped;
    for (uint8_t byte : data) {
        if (byte == static_cast<uint8_t>(TelnetCommand::IAC)) {
            escaped.append(static_cast<uint8_t>(TelnetCommand::IAC));
            escaped.append(static_cast<uint8_t>(TelnetCommand::IAC));
        } else {
            escaped.append(byte);
        }
    }

    // Append IAC EOR to terminate the GDS record
    escaped.append(static_cast<uint8_t>(TelnetCommand::IAC));
    escaped.append(static_cast<uint8_t>(TelnetCommand::EOR));

    m_socket->write(escaped);
}

} // namespace tn5250::client
