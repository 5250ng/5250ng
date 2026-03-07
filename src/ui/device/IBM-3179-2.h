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

#include <QObject>
#include <QPointer>
#include <QString>

#include "network/tn5250/message/message.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"

namespace ui::device {

/**
 * Base interface for emulated 5250 devices.
 * Provides a minimal API for attaching a screen and exchanging messages.
 */
class EmulatedDevice : public QObject {
    Q_OBJECT
  public:
    explicit EmulatedDevice(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~EmulatedDevice() = default;

    virtual QString modelName() const = 0;
    virtual int rows() const = 0;
    virtual int cols() const = 0;

    // Attach/detach screen widget
    virtual void attachScreen(ui::widgets::Q5250ScreenWidget *screen) = 0;

  signals:
    // Outbound TN5250 message ready to send to transport
    void sendMessage(const tn5250::message::Message &msg);

  public slots:
    // Inbound TN5250 message from transport
    virtual void receiveMessage(const tn5250::message::Message &msg) = 0;
};

/**
 * IBM 3179 Model 2 (24x80) emulated device.
 * Acts as a middleware to interpret TN5250 messages and update the screen.
 */
class IBM3179_2 : public EmulatedDevice {
    Q_OBJECT
  public:
    explicit IBM3179_2(QObject *parent = nullptr);

    QString modelName() const override { return QStringLiteral("IBM 3179 Model 2"); }
    int rows() const override { return 24; }
    int cols() const override { return 80; }

    void attachScreen(ui::widgets::Q5250ScreenWidget *screen) override;

  public slots:
    void receiveMessage(const tn5250::message::Message &msg) override;

  private:
    void handleClearScreen();
    void handleWriteToDisplay(const tn5250::message::command::CommandWtdWriteToDisplay &cmd);

  private:
    QPointer<ui::widgets::Q5250ScreenWidget> m_screen;
};

} // namespace ui::device
