#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>
#include <functional>

#include "commands.h"
#include "options.h"

namespace telnet {

// Telnet stream parser and negotiator. Unpacks Telnet IAC sequences and
// surfaces application data (non-Telnet bytes) via a user-provided callback.
class Client : public QObject {
    Q_OBJECT

  public:
    explicit Client(QObject *parent = nullptr);

    // Set the callback to receive application data (Telnet-unescaped payload)
    void setAppDataCallback(std::function<void(const QByteArray &)> cb);

    // Feed incoming bytes from the network into the Telnet parser
    void feed(const QByteArray &data);

    // Reset Telnet parsing state
    void reset();

  signals:
    // Negotiation events for higher-level handling (optional)
    void negotiationCommand(telnet::TelnetCommand cmd, telnet::TelnetOption opt);
    void subnegotiationReceived(telnet::TelnetOption opt, const QByteArray &data);
    void standaloneCommand(telnet::TelnetCommand cmd);

  private:
    enum class State {
        Data,                 // Passing through application data
        IAC,                  // Saw IAC, expect command
        NegotiationOption,    // After DO/DONT/WILL/WONT, expect option byte
        SubnegotiationOption, // After SB, expect option byte
        SubnegotiationData,   // Reading subnegotiation bytes
        SubnegotiationIAC     // Saw IAC inside SB, expect SE or escaped IAC
    };

    void flushAppData();

    State m_state;
    QByteArray m_appBuffer;

    // For negotiations
    telnet::TelnetCommand m_pendingCmd;
    telnet::TelnetOption m_pendingOpt;

    // For subnegotiation
    telnet::TelnetOption m_sbOpt;
    QByteArray m_sbBuffer;

    std::function<void(const QByteArray &)> m_onAppData;
};

} // namespace telnet
