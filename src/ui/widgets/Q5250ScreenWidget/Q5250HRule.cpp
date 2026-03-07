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

#include "Q5250HRule.h"
#include <QPainter>

namespace ui::widgets {

/**
 * @brief Constructs a horizontal rule widget.
 * @param parent Optional parent widget.
 *
 * Sets an expanding horizontal size policy and a fixed height of 5 pixels.
 * The rule color is initialized from the palette's Mid color.
 */
Q5250HRule::Q5250HRule(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(5);
    m_color = palette().color(QPalette::Mid);
}

/**
 * @brief Sets the color used to draw the horizontal rule.
 * @param color The new color. No-op if it equals the current color.
 *
 * Triggers a repaint when the color changes.
 */
void Q5250HRule::setColor(const QColor &color) {
    if (m_color == color)
        return;
    m_color = color;
    update();
}

/**
 * @brief Paints the horizontal rule by filling the widget rect with the current color.
 * @param event The paint event (unused).
 */
void Q5250HRule::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), m_color);
}

/**
 * @brief Returns the recommended size for the widget.
 * @return A size of 100×2 pixels (width × height).
 */
QSize Q5250HRule::sizeHint() const {
    return QSize(100, 5);
}

/**
 * @brief Returns the minimum recommended size for the widget.
 * @return A size of 10×2 pixels (width × height).
 */
QSize Q5250HRule::minimumSizeHint() const {
    return QSize(10, 5);
}

} // namespace ui::widgets
