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

#include "Q5250Cursor.h"
#include <QPainter>

namespace ui::widgets {

Q5250Cursor::Q5250Cursor(QWidget *parent) : QWidget(parent), m_color(Qt::green) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setFocusPolicy(Qt::NoFocus);
}

void Q5250Cursor::setColor(const QColor &c) {
    if (m_color == c)
        return;
    m_color = c;
    update();
}

void Q5250Cursor::setCursorShape(CursorShape shape) {
    if (m_shape == shape)
        return;
    m_shape = shape;
    update();
}

void Q5250Cursor::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);

    // DPI-aware minimum thickness so cursor remains visible on high-DPI displays
    int minPx = qMax(2, qRound(2 * devicePixelRatioF()));

    switch (m_shape) {
    case CursorShape::Block:
        p.fillRect(rect(), m_color);
        break;
    case CursorShape::Underline: {
        int h = qMax(minPx, rect().height() / 6);
        QRect underline(0, rect().height() - h, rect().width(), h);
        p.fillRect(underline, m_color);
        break;
    }
    case CursorShape::Bar: {
        int w = qMax(minPx, rect().width() / 6);
        QRect bar(0, 0, w, rect().height());
        p.fillRect(bar, m_color);
        break;
    }
    }
}

} // namespace ui::widgets
