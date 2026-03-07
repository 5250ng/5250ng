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

#include "ui/themes/terminal_theme.h"
#include <QColor>
#include <QWidget>

namespace ui::widgets {

class Q5250Cursor : public QWidget {
    Q_OBJECT
  public:
    using CursorShape = ui::themes::TerminalTheme::CursorShape;

    explicit Q5250Cursor(QWidget *parent = nullptr);
    ~Q5250Cursor() override = default;

    void setColor(const QColor &c);
    QColor color() const { return m_color; }

    void setCursorShape(CursorShape shape);
    CursorShape cursorShape() const { return m_shape; }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QColor m_color;
    CursorShape m_shape = CursorShape::Block;
};

} // namespace ui::widgets


