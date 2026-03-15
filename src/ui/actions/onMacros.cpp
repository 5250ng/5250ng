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
#include "core/scripting/script_compiler.h"
#include "core/scripting/script_executor.h"
#include "core/scripting/script_parser.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QPointer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>

static QString macrosDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + "/macros";
    QDir().mkpath(dir);
    return dir;
}

void MainWindow::onToggleMacroRecording() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->macroRecorder) {
        s->macroRecorder = new core::MacroRecorder(s->container);
    }

    // Bug fix #3: prevent recording during playback
    if (s->macroRecorder->isPlaying()) {
        ui::widgets::StyledMessageBox::information(this, "Recording",
            "Cannot record while a macro is playing.");
        return;
    }

    if (s->macroRecorder->isRecording()) {
        s->macroRecorder->stopRecording();

        // Frameless dialog for macro name input
        auto *dlg = new ui::widgets::BaseFramelessDialog(this);
        dlg->setWindowTitle("Save Macro");
        dlg->setFixedWidth(360);

        auto *label = new QLabel("Macro name:", dlg);
        auto *nameEdit = new QLineEdit(dlg);
        nameEdit->setText("Untitled");
        nameEdit->selectAll();

        auto *btnLayout = new QHBoxLayout();
        auto *saveBtn = new QPushButton("Save", dlg);
        auto *cancelBtn = new QPushButton("Discard", dlg);
        btnLayout->addStretch();
        btnLayout->addWidget(saveBtn);
        btnLayout->addWidget(cancelBtn);

        dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
        dlg->contentLayout()->setSpacing(8);
        dlg->contentLayout()->addWidget(label);
        dlg->contentLayout()->addWidget(nameEdit);
        dlg->contentLayout()->addLayout(btnLayout);

        connect(saveBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
        connect(nameEdit, &QLineEdit::returnPressed, dlg, &QDialog::accept);

        int result = dlg->exec();
        QString name = nameEdit->text().trimmed();
        delete dlg;

        if (result == QDialog::Accepted && !name.isEmpty()) {
            core::Macro macro = s->macroRecorder->finishRecording(name);
            // Bug fix #2: sanitize filename with collision avoidance
            QString safeName = core::MacroRecorder::sanitizeFileName(name);
            QString path = macrosDir() + "/" + safeName + ".json";
            int suffix = 1;
            while (QFile::exists(path))
                path = macrosDir() + "/" + safeName + "_" + QString::number(suffix++) + ".json";
            if (core::MacroRecorder::saveMacro(macro, path)) {
                ui::widgets::StyledMessageBox::information(this, "Macro Saved",
                    QString("Macro '%1' saved with %2 steps.")
                        .arg(name).arg(macro.steps.size()));
            }
        } else {
            // Bug fix #4: discard steps on cancel so they don't linger
            s->macroRecorder->discardRecording();
        }
        m_macroRecordAction->setChecked(false);
        // Update recording indicator
        if (s->macroLabel) s->macroLabel->setText("");
    } else {
        s->macroRecorder->startRecording();
        m_macroRecordAction->setChecked(true);
        // Improvement #6: visual recording indicator
        if (s->macroLabel) {
            s->macroLabel->setText("REC");
            s->macroLabel->setStyleSheet(
                "color: red; background-color: black; padding: 0px 4px; font-weight: bold;");
        }
    }
}

void MainWindow::onPlayMacro() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->macroRecorder) {
        s->macroRecorder = new core::MacroRecorder(s->container);
    }

    // Bug fix #3: prevent play during recording
    if (s->macroRecorder->isRecording()) {
        ui::widgets::StyledMessageBox::information(this, "Play Macro",
            "Cannot play while recording a macro.");
        return;
    }

    if (s->macroRecorder->isPlaying()) {
        s->macroRecorder->stopPlayback();
        if (s->macroLabel) s->macroLabel->setText("");
        return;
    }

    // Load available macros
    QVector<core::Macro> allMacros = core::MacroRecorder::loadAllMacros(macrosDir());

    // Include last recorded macro if it has steps
    if (!s->macroRecorder->lastRecordedMacro().steps.isEmpty()) {
        bool found = false;
        for (const auto &m : allMacros) {
            if (m.name == s->macroRecorder->lastRecordedMacro().name) {
                found = true;
                break;
            }
        }
        if (!found)
            allMacros.prepend(s->macroRecorder->lastRecordedMacro());
    }

    if (allMacros.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Play Macro",
            "No macros available.\n\nRecord a macro first using Macros > Record/Stop.");
        return;
    }

    // Frameless dialog to pick a macro
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("Play Macro");
    dlg->setFixedSize(400, 380);

    auto *list = new QListWidget(dlg);
    for (const auto &m : allMacros) {
        list->addItem(QString("%1  (%2 steps)").arg(m.name).arg(m.steps.size()));
    }
    if (list->count() > 0) list->setCurrentRow(0);

    // Improvement #4: repeat count
    auto *repeatLayout = new QHBoxLayout();
    repeatLayout->addWidget(new QLabel("Repeat:", dlg));
    auto *repeatSpin = new QSpinBox(dlg);
    repeatSpin->setRange(1, 9999);
    repeatSpin->setValue(1);
    repeatLayout->addWidget(repeatSpin);
    repeatLayout->addStretch();

    auto *btnLayout = new QHBoxLayout();
    auto *playBtn = new QPushButton("Play", dlg);
    auto *cancelBtn = new QPushButton("Cancel", dlg);
    btnLayout->addStretch();
    btnLayout->addWidget(playBtn);
    btnLayout->addWidget(cancelBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(new QLabel("Select a macro to play:", dlg));
    dlg->contentLayout()->addWidget(list, 1);
    dlg->contentLayout()->addLayout(repeatLayout);
    dlg->contentLayout()->addLayout(btnLayout);

    connect(playBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, dlg, &QDialog::accept);

    int result = dlg->exec();
    int idx = list->currentRow();
    int repeatCount = repeatSpin->value();
    delete dlg;

    if (result != QDialog::Accepted) return;
    if (idx < 0 || idx >= allMacros.size()) return;

    core::Macro macro = allMacros[idx];

    // Connect async playback - recorder emits steps one at a time with delays
    // Use QPointer to guard against session being closed during playback
    QPointer<ui::widgets::Q5250ScreenWidget> displayGuard = s->displayWidget;
    auto conn = std::make_shared<QMetaObject::Connection>();
    auto finishConn = std::make_shared<QMetaObject::Connection>();
    auto unlockConn = std::make_shared<QMetaObject::Connection>();

    // Bug fix #5: AID keys must go through the normal key event path so
    // buildAIDResponse() is called, instead of sending a raw byte.
    // We synthesize the original Qt key event that produces the AID byte.
    *conn = connect(s->macroRecorder, &core::MacroRecorder::playbackStep, this,
        [displayGuard](const core::MacroStep &step) {
            if (displayGuard.isNull()) return;
            if (step.type == core::MacroStep::KeyPress) {
                QKeyEvent ev(QEvent::KeyPress, step.key, step.mods, step.text);
                QApplication::sendEvent(displayGuard.data(), &ev);
            } else if (step.type == core::MacroStep::AIDKey) {
                // Replay AID keys by synthesizing the original key event so
                // the full processKeyEvent -> buildAIDResponse path is taken.
                // Map AID bytes back to Qt key events.
                int qtKey = 0;
                Qt::KeyboardModifiers mods = Qt::NoModifier;
                uint8_t aid = step.aidByte;
                if (aid == 0xF1) { qtKey = Qt::Key_Return; }
                else if (aid == 0x70) { qtKey = Qt::Key_Escape; mods = Qt::ControlModifier; } // Attn
                else if (aid == 0x71) { qtKey = Qt::Key_SysReq; } // SysReq
                else if (aid == 0x31) { qtKey = Qt::Key_F1; }
                else if (aid == 0x32) { qtKey = Qt::Key_F2; }
                else if (aid == 0x33) { qtKey = Qt::Key_F3; }
                else if (aid == 0x34) { qtKey = Qt::Key_F4; }
                else if (aid == 0x35) { qtKey = Qt::Key_F5; }
                else if (aid == 0x36) { qtKey = Qt::Key_F6; }
                else if (aid == 0x37) { qtKey = Qt::Key_F7; }
                else if (aid == 0x38) { qtKey = Qt::Key_F8; }
                else if (aid == 0x39) { qtKey = Qt::Key_F9; }
                else if (aid == 0x3A) { qtKey = Qt::Key_F10; }
                else if (aid == 0x3B) { qtKey = Qt::Key_F11; }
                else if (aid == 0x3C) { qtKey = Qt::Key_F12; }
                else if (aid == 0xB1) { qtKey = Qt::Key_F1; mods = Qt::ShiftModifier; } // F13
                else if (aid == 0xB2) { qtKey = Qt::Key_F2; mods = Qt::ShiftModifier; } // F14
                else if (aid == 0xB3) { qtKey = Qt::Key_F3; mods = Qt::ShiftModifier; } // F15
                else if (aid == 0xB4) { qtKey = Qt::Key_F4; mods = Qt::ShiftModifier; } // F16
                else if (aid == 0xB5) { qtKey = Qt::Key_F5; mods = Qt::ShiftModifier; } // F17
                else if (aid == 0xB6) { qtKey = Qt::Key_F6; mods = Qt::ShiftModifier; } // F18
                else if (aid == 0xB7) { qtKey = Qt::Key_F7; mods = Qt::ShiftModifier; } // F19
                else if (aid == 0xB8) { qtKey = Qt::Key_F8; mods = Qt::ShiftModifier; } // F20
                else if (aid == 0xB9) { qtKey = Qt::Key_F9; mods = Qt::ShiftModifier; } // F21
                else if (aid == 0xBA) { qtKey = Qt::Key_F10; mods = Qt::ShiftModifier; } // F22
                else if (aid == 0xBB) { qtKey = Qt::Key_F11; mods = Qt::ShiftModifier; } // F23
                else if (aid == 0xBC) { qtKey = Qt::Key_F12; mods = Qt::ShiftModifier; } // F24
                else if (aid == 0xF3) { qtKey = Qt::Key_F1; mods = Qt::ControlModifier; } // Help
                else if (aid == 0xF4) { qtKey = Qt::Key_PageDown; } // RollDown (PageDown -> RollDown -> 0xF4)
                else if (aid == 0xF5) { qtKey = Qt::Key_PageUp; }   // RollUp (PageUp -> RollUp -> 0xF5)
                else if (aid == 0xF6) { qtKey = Qt::Key_Print; }    // Print
                else if (aid == 0xF8) { qtKey = Qt::Key_Backspace; mods = Qt::ShiftModifier; } // Record Backspace
                else {
                    // Unknown AID - skip
                    return;
                }
                QKeyEvent ev(QEvent::KeyPress, qtKey, mods, QString());
                QApplication::sendEvent(displayGuard.data(), &ev);
            }
        });

    // Bug fix #6: wait for keyboard unlock after AID keys
    // Use QPointer guards to prevent use-after-free if session is closed during playback
    QPointer<core::MacroRecorder> recorderGuard = s->macroRecorder;
    *unlockConn = connect(s->displayWidget, &ui::widgets::Q5250ScreenWidget::terminalStateChanged, this,
        [displayGuard, recorderGuard]() {
            if (displayGuard.isNull() || recorderGuard.isNull()) return;
            if (!recorderGuard->isPlaying()) return;
            if (displayGuard->keyboardState() == ui::widgets::KeyboardState::Unlocked) {
                recorderGuard->notifyKeyboardUnlocked();
            }
        });

    // Improvement #6: playback indicator
    if (s->macroLabel) {
        s->macroLabel->setText("PLAY");
        s->macroLabel->setStyleSheet(
            "color: #00ff00; background-color: black; padding: 0px 4px; font-weight: bold;");
    }

    QPointer<QLabel> macroLabelGuard = s->macroLabel;
    *finishConn = connect(s->macroRecorder, &core::MacroRecorder::playbackFinished, this,
        [conn, finishConn, unlockConn, macroLabelGuard]() {
            QObject::disconnect(*conn);
            QObject::disconnect(*finishConn);
            QObject::disconnect(*unlockConn);
            if (!macroLabelGuard.isNull()) macroLabelGuard->setText("");
        });

    s->macroRecorder->play(macro, repeatCount);
}

void MainWindow::onManageMacros() {
    QString dir = macrosDir();

    // Bug fix #1: load macros with their file paths, use filePath for deletion
    QVector<core::Macro> macros = core::MacroRecorder::loadAllMacros(dir);
    if (macros.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Macros",
            "No saved macros found.\n\nRecord a macro using Macros > Record/Stop.");
        return;
    }

    // Frameless dialog with table of macros and management buttons
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("Manage Macros");
    dlg->setFixedSize(600, 420);

    // Improvement #2: table with editable details
    auto *table = new QTableWidget(macros.size(), 3, dlg);
    table->setHorizontalHeaderLabels({"Name", "Steps", "Description"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);

    for (int i = 0; i < macros.size(); ++i) {
        auto *nameItem = new QTableWidgetItem(macros[i].name);
        nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        table->setItem(i, 0, nameItem);

        auto *stepsItem = new QTableWidgetItem(QString::number(macros[i].steps.size()));
        stepsItem->setFlags(stepsItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(i, 1, stepsItem);

        auto *descItem = new QTableWidgetItem(macros[i].description);
        descItem->setFlags(descItem->flags() | Qt::ItemIsEditable);
        table->setItem(i, 2, descItem);
    }

    auto *pathLabel = new QLabel(QString("Macros stored in: %1").arg(dir), dlg);
    pathLabel->setWordWrap(true);
    pathLabel->setStyleSheet("color: gray; font-size: 10px;");

    auto *btnLayout = new QHBoxLayout();
    auto *deleteBtn = new QPushButton("Delete", dlg);
    auto *saveBtn = new QPushButton("Save Changes", dlg);
    auto *closeBtn = new QPushButton("Close", dlg);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addWidget(saveBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(new QLabel("Saved macros:", dlg));
    dlg->contentLayout()->addWidget(table, 1);
    dlg->contentLayout()->addWidget(pathLabel);
    dlg->contentLayout()->addLayout(btnLayout);

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    // Bug fix #1: use macro.filePath for deletion (guaranteed in sync)
    connect(deleteBtn, &QPushButton::clicked, dlg, [table, &macros]() {
        int idx = table->currentRow();
        if (idx < 0 || idx >= macros.size()) return;
        QFile::remove(macros[idx].filePath);
        table->removeRow(idx);
        macros.removeAt(idx);
    });

    // Improvement #2: save edits (name, description) back to file
    connect(saveBtn, &QPushButton::clicked, dlg, [table, &macros, dir]() {
        for (int i = 0; i < table->rowCount() && i < macros.size(); ++i) {
            QString newName = table->item(i, 0)->text().trimmed();
            QString newDesc = table->item(i, 2)->text().trimmed();
            if (newName.isEmpty()) continue;
            bool changed = (newName != macros[i].name || newDesc != macros[i].description);
            if (!changed) continue;

            // Remove old file
            if (!macros[i].filePath.isEmpty())
                QFile::remove(macros[i].filePath);

            macros[i].name = newName;
            macros[i].description = newDesc;
            QString safeName = core::MacroRecorder::sanitizeFileName(newName);
            QString newPath = dir + "/" + safeName + ".json";
            // Avoid overwriting a different macro's file
            int suffix = 1;
            while (QFile::exists(newPath)) {
                bool otherOwns = false;
                for (int j = 0; j < macros.size(); ++j) {
                    if (j != i && macros[j].filePath == newPath) { otherOwns = true; break; }
                }
                if (!otherOwns) break;
                newPath = dir + "/" + safeName + "_" + QString::number(suffix++) + ".json";
            }
            core::MacroRecorder::saveMacro(macros[i], newPath);
            macros[i].filePath = newPath;
        }
    });

    dlg->exec();
    delete dlg;
}

// Improvement #5: import macro from file
void MainWindow::onImportMacro() {
    QString filePath = QFileDialog::getOpenFileName(this, "Import Macro",
        QString(), "Macro files (*.json *.macro);;All files (*)");
    if (filePath.isEmpty()) return;

    core::Macro macro = core::MacroRecorder::loadMacro(filePath);
    if (macro.name.isEmpty() || macro.steps.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Import Macro",
            "Could not load macro from the selected file.");
        return;
    }

    QString safeName = core::MacroRecorder::sanitizeFileName(macro.name);
    QString destPath = macrosDir() + "/" + safeName + ".json";
    if (core::MacroRecorder::saveMacro(macro, destPath)) {
        ui::widgets::StyledMessageBox::information(this, "Import Macro",
            QString("Macro '%1' imported with %2 steps.")
                .arg(macro.name).arg(macro.steps.size()));
    }
}

// Improvement #5: export macro to file
void MainWindow::onExportMacro() {
    QVector<core::Macro> macros = core::MacroRecorder::loadAllMacros(macrosDir());
    if (macros.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Export Macro",
            "No macros to export.");
        return;
    }

    // Pick which macro to export
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("Export Macro");
    dlg->setFixedSize(400, 320);

    auto *list = new QListWidget(dlg);
    for (const auto &m : macros) {
        list->addItem(QString("%1  (%2 steps)").arg(m.name).arg(m.steps.size()));
    }
    if (list->count() > 0) list->setCurrentRow(0);

    auto *btnLayout = new QHBoxLayout();
    auto *exportBtn = new QPushButton("Export", dlg);
    auto *cancelBtn = new QPushButton("Cancel", dlg);
    btnLayout->addStretch();
    btnLayout->addWidget(exportBtn);
    btnLayout->addWidget(cancelBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(new QLabel("Select a macro to export:", dlg));
    dlg->contentLayout()->addWidget(list, 1);
    dlg->contentLayout()->addLayout(btnLayout);

    connect(exportBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    int result = dlg->exec();
    int idx = list->currentRow();
    delete dlg;

    if (result != QDialog::Accepted) return;
    if (idx < 0 || idx >= macros.size()) return;

    QString safeName = core::MacroRecorder::sanitizeFileName(macros[idx].name);
    QString savePath = QFileDialog::getSaveFileName(this, "Export Macro",
        safeName + ".json", "Macro files (*.json);;All files (*)");
    if (savePath.isEmpty()) return;

    if (core::MacroRecorder::saveMacro(macros[idx], savePath)) {
        ui::widgets::StyledMessageBox::information(this, "Export Macro",
            QString("Macro '%1' exported successfully.").arg(macros[idx].name));
    }
}

static QString scriptsDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + "/scripts";
    QDir().mkpath(dir);
    return dir;
}

void MainWindow::onPlayScript() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    // Check if a script is already running
    if (s->scriptExecutor && s->scriptExecutor->isRunning()) {
        s->scriptExecutor->stop();
        if (s->macroLabel) s->macroLabel->setText("");
        return;
    }

    // Collect available script files from scripts dir and allow opening any file
    QStringList scriptFiles;
    QDir dir(scriptsDir());
    for (const auto &entry : dir.entryInfoList({"*.5250script"}, QDir::Files, QDir::Name))
        scriptFiles << entry.absoluteFilePath();

    // Dialog to select script or open from file
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("Play Script");
    dlg->setFixedSize(400, 380);

    auto *list = new QListWidget(dlg);
    for (const auto &path : scriptFiles) {
        QFileInfo fi(path);
        list->addItem(fi.baseName());
    }
    if (list->count() > 0) list->setCurrentRow(0);

    auto *btnLayout = new QHBoxLayout();
    auto *browseBtn = new QPushButton("Browse...", dlg);
    auto *playBtn = new QPushButton("Play", dlg);
    auto *cancelBtn = new QPushButton("Cancel", dlg);
    btnLayout->addWidget(browseBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(playBtn);
    btnLayout->addWidget(cancelBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(new QLabel("Select a script to play:", dlg));
    dlg->contentLayout()->addWidget(list, 1);
    dlg->contentLayout()->addLayout(btnLayout);

    QString chosenPath;
    connect(browseBtn, &QPushButton::clicked, dlg, [&chosenPath, dlg]() {
        chosenPath = QFileDialog::getOpenFileName(dlg, "Open Script",
            QString(), "5250Script files (*.5250script);;All files (*)");
        if (!chosenPath.isEmpty()) dlg->accept();
    });
    connect(playBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, dlg, &QDialog::accept);

    int result = dlg->exec();
    int idx = list->currentRow();
    delete dlg;

    if (result != QDialog::Accepted) return;

    // Determine which file to load
    if (chosenPath.isEmpty()) {
        if (idx < 0 || idx >= scriptFiles.size()) return;
        chosenPath = scriptFiles[idx];
    }

    // Load script text
    QFile file(chosenPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui::widgets::StyledMessageBox::information(this, "Play Script",
            "Could not open script file.");
        return;
    }
    QString scriptText = QString::fromUtf8(file.readAll());
    file.close();

    // Parse
    core::scripting::ScriptParser parser;
    auto parseResult = parser.parse(scriptText);
    if (parseResult.hasErrors()) {
        QStringList errMsgs;
        for (const auto &e : parseResult.errors)
            errMsgs << QString("Line %1: %2").arg(e.line).arg(e.message);
        ui::widgets::StyledMessageBox::information(this, "Script Error",
            "Script has errors:\n\n" + errMsgs.join("\n"));
        return;
    }

    // Create executor
    if (!s->scriptExecutor) {
        s->scriptExecutor = new core::scripting::ScriptExecutor(s->container);
    }
    s->scriptExecutor->setScreenWidget(s->displayWidget);

    // Wire signals
    QPointer<ui::widgets::Q5250ScreenWidget> displayGuard = s->displayWidget;
    QPointer<core::scripting::ScriptExecutor> execGuard = s->scriptExecutor;
    QPointer<QLabel> macroLabelGuard = s->macroLabel;

    auto connKeyPress = std::make_shared<QMetaObject::Connection>();
    auto connAID = std::make_shared<QMetaObject::Connection>();
    auto connMoveCursor = std::make_shared<QMetaObject::Connection>();
    auto connMoveStep = std::make_shared<QMetaObject::Connection>();
    auto connMoveField = std::make_shared<QMetaObject::Connection>();
    auto connScreen = std::make_shared<QMetaObject::Connection>();
    auto connState = std::make_shared<QMetaObject::Connection>();
    auto connFinish = std::make_shared<QMetaObject::Connection>();
    auto connError = std::make_shared<QMetaObject::Connection>();
    auto connLog = std::make_shared<QMetaObject::Connection>();
    auto connPause = std::make_shared<QMetaObject::Connection>();

    // Key injection
    *connKeyPress = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::injectKeyPress, this,
        [displayGuard](int key, Qt::KeyboardModifiers mods, const QString &text) {
            if (displayGuard.isNull()) return;
            QKeyEvent ev(QEvent::KeyPress, key, mods, text);
            QApplication::sendEvent(displayGuard.data(), &ev);
        });

    // AID key injection — map AID byte to Qt key event (same logic as macro playback)
    *connAID = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::injectAIDKey, this,
        [displayGuard](uint8_t aid) {
            if (displayGuard.isNull()) return;
            int qtKey = 0;
            Qt::KeyboardModifiers mods = Qt::NoModifier;
            if (aid == 0xF1) { qtKey = Qt::Key_Return; }
            else if (aid == 0x70) { qtKey = Qt::Key_Escape; mods = Qt::ControlModifier; }
            else if (aid == 0x71) { qtKey = Qt::Key_SysReq; }
            else if (aid >= 0x31 && aid <= 0x3C) { qtKey = Qt::Key_F1 + (aid - 0x31); }
            else if (aid >= 0xB1 && aid <= 0xBC) { qtKey = Qt::Key_F1 + (aid - 0xB1); mods = Qt::ShiftModifier; }
            else if (aid == 0xF3) { qtKey = Qt::Key_F1; mods = Qt::ControlModifier; }
            else if (aid == 0xF4) { qtKey = Qt::Key_PageDown; }
            else if (aid == 0xF5) { qtKey = Qt::Key_PageUp; }
            else if (aid == 0xF6) { qtKey = Qt::Key_Print; }
            else if (aid == 0xBD) { qtKey = Qt::Key_Pause; mods = Qt::ControlModifier; }
            else return;
            QKeyEvent ev(QEvent::KeyPress, qtKey, mods, QString());
            QApplication::sendEvent(displayGuard.data(), &ev);
        });

    // Cursor movement
    *connMoveCursor = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::moveCursor, this,
        [displayGuard](int row, int col) {
            if (displayGuard.isNull()) return;
            displayGuard->moveCursor(row, col);
        });

    *connMoveStep = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::moveCursorStep, this,
        [displayGuard](const QString &dir) {
            if (displayGuard.isNull()) return;
            if (dir == "UP") displayGuard->moveCursorUp();
            else if (dir == "DOWN") displayGuard->moveCursorDown();
            else if (dir == "LEFT") displayGuard->moveCursorLeft();
            else if (dir == "RIGHT") displayGuard->moveCursorRight();
        });

    // GOTO INPUTFIELD n / NEXT / PREVIOUS
    *connMoveField = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::gotoInputField, this,
        [displayGuard](int fieldIndex) {
            if (displayGuard.isNull()) return;
            auto *buf = displayGuard->screenBuffer();
            if (!buf) return;
            const auto &fields = buf->fields();

            // Build list of input (unprotected) fields
            QVector<const ui::widgets::ScreenBuffer::Field *> inputFields;
            for (const auto &f : fields) {
                if (!f.protected_field)
                    inputFields.append(&f);
            }
            if (inputFields.isEmpty()) return;

            if (fieldIndex == -1) {
                // NEXT: find first input field after cursor
                displayGuard->moveToNextField();
            } else if (fieldIndex == -2) {
                // PREVIOUS: find input field before cursor
                displayGuard->moveToPreviousField();
            } else if (fieldIndex >= 1 && fieldIndex <= inputFields.size()) {
                // Absolute index (1-based)
                auto *f = inputFields[fieldIndex - 1];
                displayGuard->moveCursor(f->startRow, f->startCol);
            }
        });

    // Screen change notifications to executor
    *connScreen = connect(s->displayWidget->screenBuffer(), &ui::widgets::ScreenBuffer::screenChanged,
        s->scriptExecutor, &core::scripting::ScriptExecutor::notifyScreenChanged);

    *connState = connect(s->displayWidget, &ui::widgets::Q5250ScreenWidget::terminalStateChanged,
        s->scriptExecutor, &core::scripting::ScriptExecutor::notifyTerminalStateChanged);

    // Finish
    *connFinish = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::executionFinished, this,
        [=]() {
            QObject::disconnect(*connKeyPress);
            QObject::disconnect(*connAID);
            QObject::disconnect(*connMoveCursor);
            QObject::disconnect(*connMoveStep);
            QObject::disconnect(*connMoveField);
            QObject::disconnect(*connScreen);
            QObject::disconnect(*connState);
            QObject::disconnect(*connFinish);
            QObject::disconnect(*connError);
            QObject::disconnect(*connLog);
            QObject::disconnect(*connPause);
            if (!macroLabelGuard.isNull()) macroLabelGuard->setText("");
        });

    // Error
    *connError = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::executionError, this,
        [this](int line, const QString &msg) {
            ui::widgets::StyledMessageBox::information(this, "Script Error",
                QString("Line %1: %2").arg(line).arg(msg));
        });

    // Log
    *connLog = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::logMessage, this,
        [this](const QString &msg) {
            if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
                auto *s = m_sessions[m_activeIndex];
                if (s->sessionLogger)
                    s->sessionLogger->logEvent(QString("[SCRIPT] %1").arg(msg));
            }
        });

    // Pause
    *connPause = connect(s->scriptExecutor, &core::scripting::ScriptExecutor::pauseRequested, this,
        [this, execGuard]() {
            ui::widgets::StyledMessageBox::information(this, "Script Paused",
                "Script execution paused.\nClick OK to continue.");
            if (!execGuard.isNull()) execGuard->resumeAfterPause();
        });

    // Show indicator
    if (s->macroLabel) {
        s->macroLabel->setText("SCRIPT");
        s->macroLabel->setStyleSheet(
            "color: #00ccff; background-color: black; padding: 0px 4px; font-weight: bold;");
    }

    s->scriptExecutor->execute(parseResult);
}

void MainWindow::onSaveAsScript() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;

    // Load all saved macros
    QVector<core::Macro> macros = core::MacroRecorder::loadAllMacros(macrosDir());
    if (macros.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Save as Script",
            "No macros available to convert.\n\nRecord a macro first.");
        return;
    }

    // Dialog to pick a macro
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("Save Macro as Script");
    dlg->setFixedSize(400, 320);

    auto *list = new QListWidget(dlg);
    for (const auto &m : macros)
        list->addItem(QString("%1  (%2 steps)").arg(m.name).arg(m.steps.size()));
    if (list->count() > 0) list->setCurrentRow(0);

    auto *btnLayout = new QHBoxLayout();
    auto *saveBtn = new QPushButton("Save as Script", dlg);
    auto *cancelBtn = new QPushButton("Cancel", dlg);
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(new QLabel("Select a macro to convert:", dlg));
    dlg->contentLayout()->addWidget(list, 1);
    dlg->contentLayout()->addLayout(btnLayout);

    connect(saveBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    int result = dlg->exec();
    int idx = list->currentRow();
    delete dlg;

    if (result != QDialog::Accepted) return;
    if (idx < 0 || idx >= macros.size()) return;

    // Convert
    QString scriptText = core::scripting::ScriptCompiler::macroToScript(macros[idx]);

    // Save
    QString safeName = core::MacroRecorder::sanitizeFileName(macros[idx].name);
    QString defaultPath = scriptsDir() + "/" + safeName + ".5250script";
    QString savePath = QFileDialog::getSaveFileName(this, "Save Script",
        defaultPath, "5250Script files (*.5250script);;All files (*)");
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(scriptText.toUtf8());
        file.close();
        ui::widgets::StyledMessageBox::information(this, "Save as Script",
            QString("Script saved to:\n%1").arg(savePath));
    }
}
