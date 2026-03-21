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

#include "../telnet/commands.h"
#include "../telnet/options.h"
#include "client.h"
#include "logger/logger.h"
#include "tn5250/protocol_constants.h"
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
                    // EOR marks end of a TN5250 GDS record - strip it silently.
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
        } else if (option == TelnetOption::ECHO) {
            // RFC 1205: WILL ECHO signals the server is ready for 5250 data stream.
            // The client MUST accept with DO ECHO.
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::ECHO);
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Accepted ECHO (server enters 5250 data stream mode)"
            );
        } else if (option == TelnetOption::SUPPRESS_GO_AHEAD) {
            // RFC 1205: WILL SGA is part of 5250E negotiation - accept it.
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::SUPPRESS_GO_AHEAD);
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Accepted SUPPRESS_GO_AHEAD"
            );
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
        if (option == TelnetOption::TIMING_MARK) {
            // Respond WONT TIMING_MARK per RFC 860 - acknowledges host's timing request
            sendTelnetCommand(TelnetCommand::WONT, TelnetOption::TIMING_MARK);
            logger::Logger::instance()->debug("[TN5250->Client]: Responded WONT TIMING_MARK");
            break;
        }
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
        // Handle NEW_ENVIRON subnegotiation (RFC 1572 / RFC 4777)
        // Server may send:
        // - IAC SB NEW_ENVIRON SEND (0x01) [USERVAR name]... IAC SE
        // - IAC SB NEW_ENVIRON IS (0x00) [USERVAR name VALUE value]... IAC SE
        // - IAC SB NEW_ENVIRON INFO (0x02) ... IAC SE
        if (data.size() >= 1) {
            uint8_t firstByte = static_cast<uint8_t>(data[0]);
            if (firstByte == 0x01) { // SEND = 0x01 (request to send)
                // Parse requested variable names and extract IBMRSEED server seed.
                // RFC 4777: server embeds the 8-byte seed after the "IBMRSEED" name
                // in the SEND request.
                QString reqVars;
                m_serverSeed.clear();
                for (int si = 1; si < data.size(); ++si) {
                    uint8_t sb = static_cast<uint8_t>(data[si]);
                    if (sb == 0x00 || sb == 0x03) { // VAR or USERVAR
                        if (!reqVars.isEmpty()) reqVars += ", ";
                        // Read variable name + any trailing data until next marker
                        int nameStart = si + 1;
                        int nameEnd = nameStart;
                        while (nameEnd < data.size()) {
                            uint8_t nb = static_cast<uint8_t>(data[nameEnd]);
                            if (nb == 0x00 || nb == 0x01 || nb == 0x03) break;
                            nameEnd++;
                        }
                        QByteArray rawName = data.mid(nameStart, nameEnd - nameStart);
                        // Check if this is IBMRSEED with embedded seed bytes
                        QByteArray ibmrseedTag("IBMRSEED");
                        if (rawName.startsWith(ibmrseedTag) &&
                            rawName.size() > ibmrseedTag.size()) {
                            m_serverSeed = rawName.mid(ibmrseedTag.size());
                            reqVars += "USERVAR IBMRSEED + seed(" +
                                       QString::number(m_serverSeed.size()) +
                                       " bytes, hex=" +
                                       QString::fromLatin1(m_serverSeed.toHex()) + ")";
                        } else {
                            QString varName = QString::fromLatin1(rawName);
                            reqVars += (sb == 0x03 ? "USERVAR " : "VAR ") + varName;
                        }
                        si = nameEnd - 1; // loop will increment
                    }
                }
                logger::Logger::instance()->debug(
                    QString("[TN5250->Client]: NEW_ENVIRON requested by server (SEND): %1"
                            " (raw hex: %2)")
                        .arg(reqVars.isEmpty() ? "(all)" : reqVars)
                        .arg(QString::fromLatin1(data.toHex()))
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
    logger::Logger::instance()->debug(
        QString("[TN5250->Client]: sendData: %1 bytes payload, hex=%2")
            .arg(data.size())
            .arg(QString::fromLatin1(data.left(64).toHex())));

    // Wrap payload in a GDS record per RFC 1205 / SA21-9247-6:
    // [recLen(2)] [0x12A0(2)] [0x0000(2)] [varLen=0x04(1)] [flagsHi(1)] [flagsLo(1)] [opcode(1)] [payload]
    // recLen includes itself (total record size).
    // Then IAC-escape the entire record and append IAC EOR.
    using namespace tn5250::protocol;
    QByteArray record;
    int recLen = GDS_HEADER_SIZE + data.size();

    // 2-byte big-endian record length
    record.append(static_cast<char>((recLen >> 8) & 0xFF));
    record.append(static_cast<char>(recLen & 0xFF));
    // Record type 0x12A0
    record.append(static_cast<char>(GDS_RECORD_TYPE_HI));
    record.append(static_cast<char>(GDS_RECORD_TYPE_LO));
    // Reserved 0x0000
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(0x00));
    // Variable header length (includes itself: 1 + 2 flags + 1 opcode = 4)
    record.append(static_cast<char>(GDS_VAR_HDR_LEN));
    // Flags (0x00), reserved (0x00), opcode PUT_GET (0x03) - client response to host's PUT_GET
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(GDS_OPCODE_PUT_GET));
    // Payload
    record.append(data);

    sendRawData(record);
}

void TN5250Client::sendRawData(const QByteArray &data) {
    if (!m_socket) {
        return;
    }
    logger::Logger::instance()->debug(
        QString("[TN5250->Client]: sendRawData: %1 bytes (before IAC escaping)")
            .arg(data.size()));

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

void TN5250Client::sendGDS(uint8_t flagsHi, uint8_t opcode, const QByteArray &payload) {
    using namespace tn5250::protocol;
    QByteArray record;
    int recLen = GDS_HEADER_SIZE + payload.size();

    // 2-byte big-endian record length
    record.append(static_cast<char>((recLen >> 8) & 0xFF));
    record.append(static_cast<char>(recLen & 0xFF));
    // Record type 0x12A0
    record.append(static_cast<char>(GDS_RECORD_TYPE_HI));
    record.append(static_cast<char>(GDS_RECORD_TYPE_LO));
    // Reserved 0x0000
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(0x00));
    // Variable header length
    record.append(static_cast<char>(GDS_VAR_HDR_LEN));
    // Flags and opcode
    record.append(static_cast<char>(flagsHi));
    record.append(static_cast<char>(0x00));
    record.append(static_cast<char>(opcode));
    // Payload
    record.append(payload);

    sendRawData(record);
}

void TN5250Client::startHeartbeat() {
    if (!m_heartbeatTimer) {
        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(30000); // 30 seconds
        connect(m_heartbeatTimer, &QTimer::timeout, this, &TN5250Client::sendHeartbeat);
    }
    m_heartbeatTimer->start();
}

void TN5250Client::stopHeartbeat() {
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
}

void TN5250Client::sendHeartbeat() {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    // Send IAC NOP as keep-alive
    QByteArray nop;
    nop.append(static_cast<uint8_t>(TelnetCommand::IAC));
    nop.append(static_cast<uint8_t>(TelnetCommand::NOP));
    m_socket->write(nop);
    LOG_DEBUG("[TN5250->Client]: Sent heartbeat IAC NOP");
}

} // namespace tn5250::client
