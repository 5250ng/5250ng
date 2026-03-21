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

#include "network/tn5250_qt/client/client.h"
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QWidget>

namespace ui::widgets {

/**
 * Small widget that paints a filled circle in a given color.
 * Used as the traffic-light indicator in QConnectionStatusWidget.
 */
class StatusDotWidget : public QWidget {
    Q_OBJECT
  public:
    explicit StatusDotWidget(QWidget *parent = nullptr);
    void setColor(const QColor &color);
    QColor color() const { return m_color; }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QColor m_color;
};

/**
 * Status widget showing a colored circle (traffic light) and a text label.
 * The circle color reflects the connection state using UI theme colors.
 */
class QConnectionStatusWidget : public QWidget {
    Q_OBJECT
  public:
    explicit QConnectionStatusWidget(QWidget *parent = nullptr);
    ~QConnectionStatusWidget() override = default;

    void setState(tn5250::client::TN5250Client::ConnectionState state);
    tn5250::client::TN5250Client::ConnectionState state() const { return m_state; }

    void setStatusText(const QString &text);
    QString statusText() const;
    void setTextColor(const QColor &color);

  private:
    StatusDotWidget *m_dot;
    QLabel *m_text;
    QHBoxLayout *m_layout;
    tn5250::client::TN5250Client::ConnectionState m_state;

    void updateVisuals();
};

} // namespace ui::widgets
