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

#include <5250script/screen_interface.h>

namespace ui::widgets {
class Q5250ScreenWidget;
}

namespace core::scripting {

// Adapter that bridges ScreenInterface to Q5250ScreenWidget + ScreenBuffer.
// Handles EBCDIC-to-Unicode conversion so the scripting library
// does not depend on the terminal widget or EBCDIC utilities.
class ScreenBufferAdapter : public ScreenInterface {
  public:
    explicit ScreenBufferAdapter(ui::widgets::Q5250ScreenWidget *widget);

    int rows() const override;
    int cols() const override;
    int cursorRow() const override;
    int cursorCol() const override;
    QString readText(int row, int col, int length) const override;
    QString readFieldText(int row, int col) const override;
    KeyboardState keyboardState() const override;
    bool messageWaiting() const override;

  private:
    ui::widgets::Q5250ScreenWidget *m_widget;
};

} // namespace core::scripting
