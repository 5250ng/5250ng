#include "client.h"
#include "logger/logger.h"

namespace tn5250::client {

using telnet::TelnetCommand;
using telnet::TelnetOption;

void TN5250Client::performHandshake() {
    logger::Logger::instance()->debug("[TN5250->Client]: Starting TN5250 handshake");

    // Request binary mode from server
    sendTelnetCommand(TelnetCommand::DO, TelnetOption::TRANSMIT_BINARY);

    // Offer binary mode to server
    sendTelnetCommand(TelnetCommand::WILL, TelnetOption::TRANSMIT_BINARY);

    // Request EOR (End of Record) from server
    sendTelnetCommand(TelnetCommand::DO, TelnetOption::END_OF_RECORD);

    // Offer EOR to server
    sendTelnetCommand(TelnetCommand::WILL, TelnetOption::END_OF_RECORD);

    // Note: Some servers may not send explicit BINARY responses, so we'll
    // consider binary negotiated when we receive server responses or data
    // Device name will be sent when server requests it via TERMINAL_TYPE
    // subnegotiation
}

void TN5250Client::sendDeviceName() {
    // TN5250 device name negotiation
    // Format: IAC SB TERMINAL_TYPE IS <device_name> IAC SE
    // IS = 0 (we're sending our terminal type)
    QByteArray negotiation;
    negotiation.append(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.append(static_cast<uint8_t>(TelnetCommand::SB));
    negotiation.append(static_cast<uint8_t>(TelnetOption::TERMINAL_TYPE));
    negotiation.append(static_cast<uint8_t>(0)); // IS = 0
    negotiation.append(m_deviceName.toUtf8());
    negotiation.append(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.append(static_cast<uint8_t>(TelnetCommand::SE));

    if (m_socket) {
        m_socket->write(negotiation);
        m_terminalTypeSent = true;
        logger::Logger::instance()->debug(
            QString("[TN5250->Client]: Sent device name: %1").arg(m_deviceName)
        );

        // If binary wasn't explicitly negotiated via responses, assume it's
        // negotiated since we've sent DO BINARY and WILL BINARY, and server
        // accepted TERMINAL_TYPE
        if (!m_binaryNegotiated) {
            logger::Logger::instance()->debug("[TN5250->Client]: Assuming binary mode "
                                              "negotiated (no explicit response)");
            m_binaryNegotiated = true;
        }

        checkHandshakeComplete();
    }
}

void TN5250Client::sendNewEnviron() {
    // NEW_ENVIRON subnegotiation
    // Format: IAC SB NEW_ENVIRON IS VAR KBDTYPE USERVAR BRB VAR CODEPAGE USERVAR
    // 37 VAR CHARSET USERVAR 697 IAC SE IS = 0 (we're sending) VAR = 0x03
    // (variable name follows) USERVAR = 0x01 (user variable value follows)
    QByteArray negotiation;
    negotiation.append(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.append(static_cast<uint8_t>(TelnetCommand::SB));
    negotiation.append(static_cast<uint8_t>(TelnetOption::NEW_ENVIRON));
    negotiation.append(static_cast<uint8_t>(0)); // IS = 0

    // KBDTYPE = BRB (Brazilian keyboard)
    negotiation.append(static_cast<uint8_t>(0x03)); // VAR
    negotiation.append("KBDTYPE");
    negotiation.append(static_cast<uint8_t>(0x01)); // USERVAR
    negotiation.append("BRB");

    // CODEPAGE = 37
    negotiation.append(static_cast<uint8_t>(0x03)); // VAR
    negotiation.append("CODEPAGE");
    negotiation.append(static_cast<uint8_t>(0x01)); // USERVAR
    negotiation.append("37");

    // CHARSET = 697
    negotiation.append(static_cast<uint8_t>(0x03)); // VAR
    negotiation.append("CHARSET");
    negotiation.append(static_cast<uint8_t>(0x01)); // USERVAR
    negotiation.append("697");

    negotiation.append(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.append(static_cast<uint8_t>(TelnetCommand::SE));

    if (m_socket) {
        m_socket->write(negotiation);
        logger::Logger::instance()->debug("[TN5250->Client]: Sent NEW_ENVIRON: "
                                          "KBDTYPE=BRB, CODEPAGE=37, CHARSET=697");
    }
}

void TN5250Client::processHandshakeData(const QByteArray &data) {
    // Handshake completion is checked via checkHandshakeComplete()
    // which is called when negotiations complete
    // If we receive non-telnet data, the handshake is likely complete
    if (!m_handshakeComplete && data.size() > 0) {
        // If we've sent DO BINARY and WILL BINARY but haven't received explicit
        // responses, receiving data means the server accepted binary mode
        if (!m_binaryNegotiated) {
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Assuming binary mode negotiated (received data)"
            );
            m_binaryNegotiated = true;
        }
        // Receiving actual data means handshake is complete
        checkHandshakeComplete();
    }
}

void TN5250Client::checkHandshakeComplete() {
    // Handshake is complete when:
    // 1. Binary mode is negotiated (required)
    // 2. We've either sent terminal type OR received data OR server didn't
    // request terminal type Note: EOR is optional, so we don't require it Note:
    // Some servers may not request terminal type, so we complete after binary +
    // data
    if (!m_handshakeComplete && m_binaryNegotiated) {
        // Complete handshake if:
        // - Terminal type was sent (server requested it and we responded), OR
        // - We've received data (server is sending screen data, handshake is done),
        // OR
        // - Server accepted TERMINAL_TYPE but hasn't requested it yet (give it a
        // moment)
        //   In this case, we'll complete when we receive data
        bool shouldComplete = false;

        if (m_terminalTypeSent) {
            // We sent terminal type, handshake should be complete
            shouldComplete = true;
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Handshake complete - terminal type sent"
            );
        } else if (m_handshakeBuffer.size() > 0) {
            // We received data, server is ready
            shouldComplete = true;
            logger::Logger::instance()->debug(
                "[TN5250->Client]: Handshake complete - data received"
            );
        }
        // Note: If server doesn't request terminal type, we'll complete when data
        // arrives

        if (shouldComplete) {
            m_handshakeComplete = true;
            setState(ConnectionState::Connected);
            emit connected();
            logger::Logger::instance()->debug("[TN5250->Client]: Handshake complete");

            // Process any buffered data
            if (m_handshakeBuffer.size() > 0) {
                emit dataReceived(m_handshakeBuffer);
                m_handshakeBuffer.clear();
            }
        }
    }
}

} // namespace tn5250::client
