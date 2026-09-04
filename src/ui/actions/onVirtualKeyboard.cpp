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
#include <QtUiStyle/BaseFramelessDialog.h>
#include <QVBoxLayout>

void MainWindow::onToggleVirtualKeyboard() {
    bool firstShow = false;
    if (!m_virtualKeyboardWindow) {
        firstShow = true;
        m_virtualKeyboardWindow = new qt_ui_style::BaseFramelessDialog(this);
        m_virtualKeyboardWindow->setWindowTitle(tr("Virtual 5250 Keyboard"));
        m_virtualKeyboardWindow->setWindowFlag(Qt::Tool, true);
        m_virtualKeyboardWindow->setWindowModality(Qt::NonModal);
        m_virtualKeyboardWindow->setAttribute(Qt::WA_QuitOnClose, false);

        m_virtualKeyboard = new ui::widgets::QVirtualKeyboardWidget(m_virtualKeyboardWindow);
        m_virtualKeyboardWindow->contentLayout()->addWidget(m_virtualKeyboard);
        connect(m_virtualKeyboard, &ui::widgets::QVirtualKeyboardWidget::actionTriggered,
                this, &MainWindow::onVirtualKeyboardAction);
        connect(m_virtualKeyboard, &ui::widgets::QVirtualKeyboardWidget::characterTriggered,
                this, &MainWindow::onVirtualKeyboardCharacter);
        connect(m_virtualKeyboard, &ui::widgets::QVirtualKeyboardWidget::remapRequested,
                this, &MainWindow::onVirtualKeyboardRemap);

        connect(m_virtualKeyboardWindow, &QDialog::finished, this, [this]() {
            if (m_virtualKeyboardAction) m_virtualKeyboardAction->setChecked(false);
            rebindVirtualKeyboardPulse();
        });
    }

    if (m_virtualKeyboardWindow->isVisible()) {
        m_virtualKeyboardWindow->reject();
        return;
    }

    m_virtualKeyboard->refreshChordLabels();
    if (firstShow) {
        m_virtualKeyboardWindow->resize(900, 380);
        m_virtualKeyboardWindow->move(
            frameGeometry().center() - m_virtualKeyboardWindow->rect().center());
    }
    m_virtualKeyboardWindow->show();
    m_virtualKeyboardWindow->raise();
    m_virtualKeyboardWindow->activateWindow();
    if (m_virtualKeyboardAction) m_virtualKeyboardAction->setChecked(true);
    rebindVirtualKeyboardPulse();
}

void MainWindow::rebindVirtualKeyboardPulse() {
    // Drop the previous session's connection (harmless if it is already stale).
    if (m_virtualKeyboardPulseConnection) {
        QObject::disconnect(m_virtualKeyboardPulseConnection);
        m_virtualKeyboardPulseConnection = QMetaObject::Connection();
    }
    if (!m_virtualKeyboardWindow || !m_virtualKeyboardWindow->isVisible()
        || !m_virtualKeyboard || !m_displayWidget) {
        return;
    }
    m_virtualKeyboardPulseConnection = connect(
        m_displayWidget, &ui::widgets::Q5250ScreenWidget::keyRecorded,
        m_virtualKeyboard,
        [this](int key, Qt::KeyboardModifiers mods, const QString &) {
            m_virtualKeyboard->pulseForChord(key, mods);
        });
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
