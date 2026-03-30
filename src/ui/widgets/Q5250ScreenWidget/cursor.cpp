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

#include "Q5250ScreenWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

namespace ui::widgets {

void Q5250ScreenWidget::setCursorBlinkRate(int msec) {
    m_cursorBlinkRate = msec;
    if (m_blinkTimer) {
        if (msec <= 0) {
            // Stop blinking: keep cursor always visible
            m_blinkTimer->stop();
            m_cursorBlinkState = true;
            updateCursorWidget();
            update();
        } else {
            m_blinkTimer->setInterval(msec);
            if (!m_blinkTimer->isActive()) {
                m_blinkTimer->start(msec);
            }
        }
    }
}

void Q5250ScreenWidget::setCursorEnabled(bool enabled) {
    m_cursorEnabled = enabled;
    if (m_cursorWidget) {
        m_cursorWidget->setVisible(enabled);
    }
    update();
}

} // namespace ui::widgets
