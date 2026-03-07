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

#include <QWidget>
#include <QVBoxLayout>
#include "Q5250ScreenWidget.h"
#include "Q5250HRule.h"
#include "ui/themes/terminal_theme.h"

namespace ui::widgets {

class Q5250TerminalView : public QWidget {
    Q_OBJECT
  public:
    explicit Q5250TerminalView(QWidget *parent = nullptr);
    ~Q5250TerminalView() override = default;

    Q5250ScreenWidget *screen() const { return m_screen; }
    Q5250HRule *rule() const { return m_rule; }

    void setScreenSize(int rows, int cols);
    void setFont(const QFont &font);
    void applyTerminalTheme(const ui::themes::TerminalTheme &theme);

  private:
    QVBoxLayout *m_layout;
    Q5250ScreenWidget *m_screen;
    Q5250HRule *m_rule;
};

} // namespace ui::widgets
