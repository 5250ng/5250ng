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

#include "../main_window.h"
#include "ui/dialogs/keyboard_remap_dialog.h"
#include <QVBoxLayout>

void MainWindow::onToggleVirtualKeyboard() {
    if (!m_virtualKeyboard) {
        m_virtualKeyboard = new ui::widgets::QVirtualKeyboardWidget(this);
        connect(m_virtualKeyboard, &ui::widgets::QVirtualKeyboardWidget::actionTriggered,
                this, &MainWindow::onVirtualKeyboardAction);
        connect(m_virtualKeyboard, &ui::widgets::QVirtualKeyboardWidget::characterTriggered,
                this, &MainWindow::onVirtualKeyboardCharacter);
        connect(m_virtualKeyboard, &ui::widgets::QVirtualKeyboardWidget::remapRequested,
                this, &MainWindow::onVirtualKeyboardRemap);
        // Attach at the bottom of the main content area.
        QVBoxLayout *root = contentLayout();
        if (root) {
            // Insert before the last widget (global bottom status bar).
            int insertAt = root->count() > 0 ? root->count() - 1 : 0;
            root->insertWidget(insertAt, m_virtualKeyboard);
        }
    }
    bool show = !m_virtualKeyboard->isVisible();
    m_virtualKeyboard->setVisible(show);
    if (m_virtualKeyboardAction) m_virtualKeyboardAction->setChecked(show);
}

void MainWindow::onEditKeyboardMapping() {
    ui::dialogs::KeyboardRemapDialog dlg(this);
    connect(&dlg, &ui::dialogs::KeyboardRemapDialog::mappingChanged, this, [this]() {
        if (m_virtualKeyboard) m_virtualKeyboard->refreshChordLabels();
    });
    dlg.exec();
}

void MainWindow::onVirtualKeyboardAction(core::MappedAction action) {
    if (m_displayWidget) {
        m_displayWidget->setFocus();
        m_displayWidget->dispatchMappedAction(action);
    }
}

void MainWindow::onVirtualKeyboardCharacter(QChar ch) {
    if (m_displayWidget) {
        m_displayWidget->setFocus();
        m_displayWidget->dispatchCharacter(ch);
    }
}

void MainWindow::onVirtualKeyboardRemap(core::MappedAction action) {
    ui::dialogs::KeyboardRemapDialog dlg(this, action);
    connect(&dlg, &ui::dialogs::KeyboardRemapDialog::mappingChanged, this, [this]() {
        if (m_virtualKeyboard) m_virtualKeyboard->refreshChordLabels();
    });
    dlg.exec();
}
