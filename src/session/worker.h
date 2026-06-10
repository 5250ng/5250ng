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

#include "core/pcap_replay.h"
#include "network/tn5250_qt/client/client.h"
#include "session/config.h"
#include <QByteArray>
#include <QObject>

class QTimer;

namespace tn5250::session {

class Worker : public QObject {
    Q_OBJECT
  public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker();

    void setConfig(const ::session::SessionConfig &cfg);

  public slots:
    void start();
    void stop();
    void sendInput(const QByteArray &data);
    void sendAttention();
    void sendSystemRequest();
    void sendGDS(uint8_t flagsHi, uint8_t opcode, const QByteArray &payload);

  signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void stateChanged(tn5250::client::TN5250Client::ConnectionState state);
    void appData(const QByteArray &data);
    // Emitted by replay sessions once the last captured record was delivered.
    void replayFinished();

  private:
    ::session::SessionConfig m_config;
    tn5250::client::TN5250Client *m_client;

    // PCAP replay state (active when m_config.isReplay())
    QTimer *m_replayTimer = nullptr;
    QVector<core::pcap::ReplayRecord> m_replayRecords;
    int m_replayIndex = 0;

    void onClientData(const QByteArray &data);
    void startReplay();
    void onReplayTick();
};

} // namespace tn5250::session
