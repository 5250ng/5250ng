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

#include "core/keyboard_mapping.h"
#include <QChar>
#include <QList>
#include <QPushButton>
#include <QString>
#include <QWidget>

class QGridLayout;

namespace ui::widgets {

// Clickable on-screen 5250 enhanced keyboard (IBM Model M style).
//
// Emits actionTriggered() when the user clicks a function / control key and
// characterTriggered() for alphanumerics. The owning main window is expected
// to route those signals to the active Q5250ScreenWidget.
class QVirtualKeyboardWidget : public QWidget {
    Q_OBJECT

  public:
    explicit QVirtualKeyboardWidget(QWidget *parent = nullptr);

    // Re-label action keys after a remap so chord hints match current bindings.
    void refreshChordLabels();

  signals:
    void actionTriggered(core::MappedAction action);
    void characterTriggered(QChar ch);
    void remapRequested(core::MappedAction action);

  private:
    void buildLayout();
    QPushButton *addActionKey(QGridLayout *grid, int row, int col, int rowSpan, int colSpan,
                              const QString &label, core::MappedAction action);
    QPushButton *addCharKey(QGridLayout *grid, int row, int col, int rowSpan, int colSpan,
                            QChar unshifted, QChar shifted);
    QPushButton *addModifierKey(QGridLayout *grid, int row, int col, int rowSpan, int colSpan,
                                const QString &label, Qt::KeyboardModifier mod);

    QChar applyModifiers(QChar unshifted, QChar shifted) const;

    // Track action buttons so we can refresh their chord sub-label.
    struct ActionButton {
        QPushButton *button;
        core::MappedAction action;
        QString baseLabel;
    };
    QList<ActionButton> m_actionButtons;

    bool m_shiftHeld = false;
    bool m_ctrlHeld = false;
    bool m_altHeld = false;
    QPushButton *m_shiftBtn = nullptr;
    QPushButton *m_ctrlBtn = nullptr;
    QPushButton *m_altBtn = nullptr;
};

} // namespace ui::widgets
