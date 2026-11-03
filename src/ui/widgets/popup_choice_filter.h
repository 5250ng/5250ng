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

#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QObject>

namespace ui::widgets {

/// Event filter for a combo box whose choice triggers an action (opening a
/// session): QComboBox changes the current item and emits activated() for
/// the mouse wheel, arrow keys and type-ahead while the popup is closed, so
/// scrolling over it would run the action for every entry passed. This
/// filter drops those events; the user picks from the popup, which can still
/// be opened with a click, Space, F4 or Alt+Down. Other keys are consumed too,
/// so they do not reach the combo's parent (shortcuts still work).
class PopupChoiceFilter : public QObject {
  public:
    explicit PopupChoiceFilter(QComboBox *combo) : QObject(combo) {
        combo->installEventFilter(this);
    }

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Wheel)
            return true;
        if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            switch (key->key()) {
            case Qt::Key_Space:
            case Qt::Key_F4:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
            case Qt::Key_Escape:
                return false;
            case Qt::Key_Down:
                // Alt+Down opens the popup (Alt+Up only moves the selection)
                return !(key->modifiers() & Qt::AltModifier);
            default:
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace ui::widgets
