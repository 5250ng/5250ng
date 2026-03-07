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

#include "Q5250TerminalView.h"

namespace ui::widgets {

Q5250TerminalView::Q5250TerminalView(QWidget *parent)
    : QWidget(parent),
      m_layout(new QVBoxLayout(this)),
      m_screen(new Q5250ScreenWidget(this)),
      m_rule(new Q5250HRule(this)) {
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_screen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_layout->addWidget(m_screen, 1);
    m_layout->addWidget(m_rule);
    setLayout(m_layout);
}

void Q5250TerminalView::setScreenSize(int rows, int cols) {
    m_screen->setScreenSize(rows, cols);
}

void Q5250TerminalView::setFont(const QFont &font) {
    m_screen->setFont(font);
}

void Q5250TerminalView::applyTerminalTheme(const ui::themes::TerminalTheme &theme) {
    bool transparent = (theme.backgroundMode == ui::themes::TerminalTheme::Image
                        && theme.screenBackgroundOpacity < 1.0);
    setAttribute(Qt::WA_TranslucentBackground, transparent);
    setAutoFillBackground(false);

    m_rule->setAttribute(Qt::WA_TranslucentBackground, transparent);
    m_rule->setAutoFillBackground(false);

    m_screen->applyTerminalTheme(theme);

    QColor ruleColor = theme.hruleColor.isValid() ? theme.hruleColor : theme.colorGreen;
    ruleColor = theme.adjustColor(ruleColor);
    m_rule->setColor(ruleColor);
}

} // namespace ui::widgets
