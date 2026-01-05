#include "client.h"
#include "logger/logger.h"

namespace telnet {

Client::Client(QObject *parent)
    : QObject(parent), m_state(State::Data), m_pendingCmd(TelnetCommand::IAC),
      m_pendingOpt(TelnetOption::TRANSMIT_BINARY),
      m_sbOpt(TelnetOption::TRANSMIT_BINARY) {}

void Client::setAppDataCallback(std::function<void(const QByteArray &)> cb) {
    m_onAppData = std::move(cb);
}

void Client::flushAppData() {
    if (!m_appBuffer.isEmpty() && m_onAppData) {
        m_onAppData(m_appBuffer);
    }
    m_appBuffer.clear();
}

void Client::reset() {
    m_state = State::Data;
    m_appBuffer.clear();
    m_sbBuffer.clear();
}

void Client::feed(const QByteArray &data) {
    for (uint8_t b : data) {
        switch (m_state) {
        case State::Data:
            if (b == static_cast<uint8_t>(TelnetCommand::IAC)) {
                flushAppData();
                m_state = State::IAC;
            } else {
                m_appBuffer.append(static_cast<char>(b));
            }
            break;

        case State::IAC: {
            TelnetCommand cmd = static_cast<TelnetCommand>(b);
            if (cmd == TelnetCommand::IAC) {
                // Escaped 0xFF within data
                m_appBuffer.append(static_cast<char>(0xFF));
                m_state = State::Data;
            } else if (cmd == TelnetCommand::SB) {
                m_state = State::SubnegotiationOption;
            } else if (cmd == TelnetCommand::SE) {
                // Unexpected SE; ignore and return to data
                m_state = State::Data;
            } else if (isStandaloneTelnetCommand(cmd)) {
                emit standaloneCommand(cmd);
                m_state = State::Data;
            } else {
                // Negotiation command (DO/DONT/WILL/WONT)
                m_pendingCmd = cmd;
                m_state = State::NegotiationOption;
            }
        } break;

        case State::NegotiationOption: {
            m_pendingOpt = static_cast<TelnetOption>(b);
            emit negotiationCommand(m_pendingCmd, m_pendingOpt);
            m_state = State::Data;
        } break;

        case State::SubnegotiationOption: {
            m_sbOpt = static_cast<TelnetOption>(b);
            m_sbBuffer.clear();
            m_state = State::SubnegotiationData;
        } break;

        case State::SubnegotiationData:
            if (b == static_cast<uint8_t>(TelnetCommand::IAC)) {
                m_state = State::SubnegotiationIAC;
            } else {
                m_sbBuffer.append(static_cast<char>(b));
            }
            break;

        case State::SubnegotiationIAC: {
            TelnetCommand sbCmd = static_cast<TelnetCommand>(b);
            if (sbCmd == TelnetCommand::IAC) {
                // Escaped IAC inside SB
                m_sbBuffer.append(static_cast<char>(0xFF));
                m_state = State::SubnegotiationData;
            } else if (sbCmd == TelnetCommand::SE) {
                // End of SB
                emit subnegotiationReceived(m_sbOpt, m_sbBuffer);
                m_sbBuffer.clear();
                m_state = State::Data;
            } else {
                // Unexpected; ignore and continue subnegotiation
                m_state = State::SubnegotiationData;
            }
        } break;
        }
    }
    flushAppData();
}

} // namespace telnet
