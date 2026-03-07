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

#include "QBackgroundImageWidget.h"
#include <QPainter>

namespace ui::widgets {

QBackgroundImageWidget::QBackgroundImageWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
}

void QBackgroundImageWidget::setImage(const QPixmap &pixmap) {
    m_image = pixmap;
    setVisible(!m_image.isNull());
    update();
}

void QBackgroundImageWidget::setImage(const QByteArray &data) {
    QPixmap px;
    px.loadFromData(data);
    setImage(px);
}

void QBackgroundImageWidget::clearImage() {
    m_image = QPixmap();
    setVisible(false);
    update();
}

void QBackgroundImageWidget::setLayout(
    ui::themes::TerminalTheme::BackgroundImageLayout layout) {
    m_layout = layout;
    update();
}

void QBackgroundImageWidget::setImageOpacity(double opacity) {
    m_opacity = opacity;
    update();
}

void QBackgroundImageWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (m_image.isNull()) return;

    QPainter painter(this);
    painter.setOpacity(m_opacity);

    switch (m_layout) {
    case ui::themes::TerminalTheme::Stretch:
        painter.drawPixmap(rect(), m_image);
        break;
    case ui::themes::TerminalTheme::Fit: {
        QPixmap scaled = m_image.scaled(size(), Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
        break;
    }
    case ui::themes::TerminalTheme::Center: {
        int x = (width() - m_image.width()) / 2;
        int y = (height() - m_image.height()) / 2;
        painter.drawPixmap(x, y, m_image);
        break;
    }
    case ui::themes::TerminalTheme::Tile:
        for (int y = 0; y < height(); y += m_image.height()) {
            for (int x = 0; x < width(); x += m_image.width()) {
                painter.drawPixmap(x, y, m_image);
            }
        }
        break;
    }
}

} // namespace ui::widgets
