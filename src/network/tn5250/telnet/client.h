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
