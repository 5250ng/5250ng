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

#include "QConnectionStatusWidget.h"
#include "ui/themes/manager.h"
#include <QFontMetrics>
#include <QPainter>
#include <QtWidgets/QStyle>

namespace ui::widgets {

QConnectionStatusWidget::QConnectionStatusWidget(QWidget *parent)
    : QWidget(parent),
      m_dot(new StatusDotWidget(this)),
      m_text(new QLabel(this)),
      m_layout(new QHBoxLayout(this)),
      m_state(tn5250::client::TN5250Client::ConnectionState::Disconnected) {
    m_dot->setFixedSize(12, 12);

    m_text->setText("Not connected");
    // Use palette text color (inherits from UI theme)
    m_text->setStyleSheet("background: transparent;");

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);
    m_layout->addWidget(m_dot, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_text, 0, Qt::AlignVCenter);
    setLayout(m_layout);

    updateVisuals();
}

void QConnectionStatusWidget::setState(tn5250::client::TN5250Client::ConnectionState state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    updateVisuals();
}

void QConnectionStatusWidget::setStatusText(const QString &text) {
    m_text->setText(text);
}

QString QConnectionStatusWidget::statusText() const {
    return m_text->text();
}

void QConnectionStatusWidget::setTextColor(const QColor &color) {
    m_text->setStyleSheet(
        QString("color: %1; background: transparent;").arg(color.name(QColor::HexRgb)));
}

void QConnectionStatusWidget::updateVisuals() {
    QString stateProp;
    QString tooltip;
    QString text = m_text->text();
    switch (m_state) {
    case tn5250::client::TN5250Client::ConnectionState::Disconnected:
        stateProp = "disconnected";
        tooltip = "Not connected";
        if (text.isEmpty() || text == "Ready")
            text = "Not connected";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Connecting:
        stateProp = "connecting";
        tooltip = "Connecting";
        if (text.isEmpty())
            text = "Connecting";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Negotiating:
        stateProp = "negotiating";
        tooltip = "Waiting for system";
        if (text.isEmpty())
            text = "Waiting for system";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Connected:
        stateProp = "connected";
        tooltip = "Ready";
        if (text.isEmpty() || text == "Not connected")
            text = "Ready";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Error:
        stateProp = "error";
        tooltip = "Error";
        if (text.isEmpty())
            text = "Error";
        break;
    }

    // Resolve dot color from UI theme
    auto &tm = ui::themes::ThemeManager::instance();
    QString colorKey = "status." + stateProp;
    QString fallback = (stateProp == "connected")    ? "#2e7d32"
                       : (stateProp == "error" || stateProp == "disconnected") ? "#d32f2f"
                                                     : "#f57c00";
    QColor dotColor(tm.color(colorKey, fallback));
    m_dot->setColor(dotColor);

    m_dot->setToolTip(tooltip);
    m_text->setText(text);
}

// --- StatusDotWidget ---

StatusDotWidget::StatusDotWidget(QWidget *parent)
    : QWidget(parent), m_color(Qt::gray) {}

void StatusDotWidget::setColor(const QColor &color) {
    if (m_color != color) {
        m_color = color;
        update();
    }
}

void StatusDotWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(m_color);
    // Draw a filled circle fitting within the widget
    const int d = qMin(width(), height());
    const QRectF r((width() - d) / 2.0, (height() - d) / 2.0, d, d);
    p.drawEllipse(r);
}

} // namespace ui::widgets
