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

void Q5250ScreenWidget::setMatchReplaceEngine(core::MatchReplaceEngine *engine) {
    m_matchReplace = engine;
}

void Q5250ScreenWidget::refreshMatchReplaceOverlay() {
    if (!m_matchReplace || !m_screenBuffer) return;
    int rows = m_screenBuffer->rows();
    int cols = m_screenBuffer->cols();
    QVector<QString> decodedLines;
    decodedLines.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        QString line;
        line.reserve(cols);
        for (int c = 0; c < cols; ++c) {
            uint8_t ch = m_screenBuffer->character(r, c);
            if (ch == 0x00)
                line.append(QChar(' '));
            else
                line.append(core::EBCDIC::ebcdicToChar(ch));
        }
        decodedLines.append(line);
    }
    m_matchReplace->rebuildOverlay(decodedLines, rows, cols);
}

void Q5250ScreenWidget::onScreenChanged() {
    // Push to screen history and refresh hotspots on each screen update
    pushScreenToHistory();
    if (m_hotspotDetector.isEnabled())
        refreshHotspots();
    // Rebuild match-and-replace overlay
    if (m_matchReplace && m_matchReplace->isEnabled()) {
        refreshMatchReplaceOverlay();
    }
    update();
}

void Q5250ScreenWidget::onCursorMoved(const QPoint &pos) {
    Q_UNUSED(pos);
    updateCursorWidget();
    update();
}

void Q5250ScreenWidget::onBlinkTimer() {
    m_cursorBlinkState = !m_cursorBlinkState;
    m_blinkTextState = !m_blinkTextState;
    updateCursorWidget();
    update();
}

} // namespace ui::widgets
