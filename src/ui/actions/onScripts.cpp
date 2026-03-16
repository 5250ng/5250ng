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
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

static QString macrosDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + "/macros";
    QDir().mkpath(dir);
    return dir;
}

static QString scriptsDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + "/scripts";
    QDir().mkpath(dir);
    return dir;
}

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

void MainWindow::onRecordScript() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->macroRecorder) {
        s->macroRecorder = new core::MacroRecorder(s->container);
    }

    // Prevent recording during playback
    if (s->macroRecorder->isPlaying()) {
        ui::widgets::StyledMessageBox::information(this, "Recording",
            "Cannot record while a macro is playing.");
        m_scriptRecordAction->setChecked(false);
        return;
    }

    if (s->macroRecorder->isRecording()) {
        // Stop recording
        s->macroRecorder->stopRecording();

        // Ask for script name
        auto *dlg = new ui::widgets::BaseFramelessDialog(this);
        dlg->setWindowTitle("Save Script");
        dlg->setFixedWidth(360);

        auto *label = new QLabel("Script name:", dlg);
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
            // Convert to .5250script and save directly
            QString scriptText = core::scripting::ScriptCompiler::macroToScript(macro);
            QString safeName = core::MacroRecorder::sanitizeFileName(name);
            QString path = scriptsDir() + "/" + safeName + ".5250script";
            int suffix = 1;
            while (QFile::exists(path))
                path = scriptsDir() + "/" + safeName + "_" + QString::number(suffix++) + ".5250script";
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(scriptText.toUtf8());
                file.close();
                ui::widgets::StyledMessageBox::information(this, "Script Saved",
                    QString("Script '%1' saved with %2 steps.")
                        .arg(name).arg(macro.steps.size()));
            }
        } else {
            s->macroRecorder->discardRecording();
        }
        m_scriptRecordAction->setChecked(false);
        m_scriptStopAction->setEnabled(false);
        if (s->macroLabel) s->macroLabel->setText("");
    } else {
        // Start recording
        s->macroRecorder->startRecording();
        m_scriptRecordAction->setChecked(true);
        m_scriptStopAction->setEnabled(true);
        if (s->macroLabel) {
            s->macroLabel->setText("REC");
            s->macroLabel->setStyleSheet(
                "color: red; background-color: black; padding: 0px 4px; font-weight: bold;");
        }
    }
}

// ---------------------------------------------------------------------------
// Stop (recording or playback)
// ---------------------------------------------------------------------------

void MainWindow::onStopExecution() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (s->macroRecorder && s->macroRecorder->isRecording()) {
        s->macroRecorder->stopRecording();
        s->macroRecorder->discardRecording();
        m_scriptRecordAction->setChecked(false);
    }
    if (s->macroRecorder && s->macroRecorder->isPlaying()) {
        s->macroRecorder->stopPlayback();
    }
    if (s->scriptExecutor && s->scriptExecutor->isRunning()) {
        s->scriptExecutor->stop();
    }
    m_scriptStopAction->setEnabled(false);
    if (s->macroLabel) s->macroLabel->setText("");
}

// ---------------------------------------------------------------------------
// Dynamic Scripts submenu
// ---------------------------------------------------------------------------

void MainWindow::rebuildScriptsSubmenu() {
    m_scriptsSubmenu->clear();

    QDir dir(scriptsDir());
    QFileInfoList entries = dir.entryInfoList({"*.5250script"}, QDir::Files, QDir::Name);

    if (entries.isEmpty()) {
        QAction *empty = m_scriptsSubmenu->addAction("(No scripts)");
        empty->setEnabled(false);
        return;
    }

    for (const QFileInfo &fi : entries) {
        QString path = fi.absoluteFilePath();
        QString name = fi.baseName();
        QMenu *sub = m_scriptsSubmenu->addMenu(name);
        connect(sub->addAction("Run"), &QAction::triggered, this,
            [this, path]() { onRunScript(path); });
        connect(sub->addAction("Edit"), &QAction::triggered, this,
            [this, path]() { onEditScript(path); });
        connect(sub->addAction("Delete"), &QAction::triggered, this,
            [this, path]() { onDeleteScript(path); });
    }
}

// ---------------------------------------------------------------------------
// Run script
// ---------------------------------------------------------------------------

void MainWindow::onRunScript(const QString &path) {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    // Stop any running script first
    if (s->scriptExecutor && s->scriptExecutor->isRunning()) {
        s->scriptExecutor->stop();
        if (s->macroLabel) s->macroLabel->setText("");
    }

    // Load script text
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui::widgets::StyledMessageBox::information(this, "Run Script",
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

    // AID key injection
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

            QVector<const ui::widgets::ScreenBuffer::Field *> inputFields;
            for (const auto &f : fields) {
                if (!f.protected_field)
                    inputFields.append(&f);
            }
            if (inputFields.isEmpty()) return;

            if (fieldIndex == -1) {
                displayGuard->moveToNextField();
            } else if (fieldIndex == -2) {
                displayGuard->moveToPreviousField();
            } else if (fieldIndex >= 1 && fieldIndex <= inputFields.size()) {
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
            if (m_scriptStopAction) m_scriptStopAction->setEnabled(false);
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
    m_scriptStopAction->setEnabled(true);

    s->scriptExecutor->execute(parseResult);
}

// ---------------------------------------------------------------------------
// Edit script (open in external editor)
// ---------------------------------------------------------------------------

void MainWindow::onEditScript(const QString &path) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// ---------------------------------------------------------------------------
// Delete script
// ---------------------------------------------------------------------------

void MainWindow::onDeleteScript(const QString &path) {
    QFileInfo fi(path);
    auto answer = ui::widgets::StyledMessageBox::question(this, "Delete Script",
        QString("Delete script '%1'?").arg(fi.baseName()));
    if (answer == ui::widgets::StyledMessageBox::Yes) {
        QFile::remove(path);
    }
}

// ---------------------------------------------------------------------------
// New Script (create template and open)
// ---------------------------------------------------------------------------

void MainWindow::onNewScript() {
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("New Script");
    dlg->setFixedWidth(360);

    auto *label = new QLabel("Script name:", dlg);
    auto *nameEdit = new QLineEdit(dlg);
    nameEdit->setText("my_script");
    nameEdit->selectAll();

    auto *btnLayout = new QHBoxLayout();
    auto *createBtn = new QPushButton("Create", dlg);
    auto *cancelBtn = new QPushButton("Cancel", dlg);
    btnLayout->addStretch();
    btnLayout->addWidget(createBtn);
    btnLayout->addWidget(cancelBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(label);
    dlg->contentLayout()->addWidget(nameEdit);
    dlg->contentLayout()->addLayout(btnLayout);

    connect(createBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(nameEdit, &QLineEdit::returnPressed, dlg, &QDialog::accept);

    int result = dlg->exec();
    QString name = nameEdit->text().trimmed();
    delete dlg;

    if (result != QDialog::Accepted || name.isEmpty()) return;

    QString safeName = core::MacroRecorder::sanitizeFileName(name);
    QString path = scriptsDir() + "/" + safeName + ".5250script";
    int suffix = 1;
    while (QFile::exists(path))
        path = scriptsDir() + "/" + safeName + "_" + QString::number(suffix++) + ".5250script";

    // Write a starter template
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QString(
            "// %1\n"
            "// Created with 5250ng\n"
            "\n"
            "WAIT SCREEN\n"
            "TYPE \"hello\"\n"
            "PRESS ENTER\n"
        ).arg(name).toUtf8());
        file.close();
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// ---------------------------------------------------------------------------
// Import Script (copy .5250script into scripts dir)
// ---------------------------------------------------------------------------

void MainWindow::onImportScript() {
    QString filePath = QFileDialog::getOpenFileName(this, "Import Script",
        QString(), "5250Script files (*.5250script);;All files (*)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    QString destPath = scriptsDir() + "/" + fi.fileName();
    int suffix = 1;
    while (QFile::exists(destPath)) {
        destPath = scriptsDir() + "/" + fi.baseName() + "_" + QString::number(suffix++) + ".5250script";
    }

    if (QFile::copy(filePath, destPath)) {
        ui::widgets::StyledMessageBox::information(this, "Import Script",
            QString("Script '%1' imported.").arg(fi.baseName()));
    } else {
        ui::widgets::StyledMessageBox::information(this, "Import Script",
            "Could not import script file.");
    }
}

// ---------------------------------------------------------------------------
// Open Scripts Folder
// ---------------------------------------------------------------------------

void MainWindow::onOpenScriptsFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsDir()));
}

// ---------------------------------------------------------------------------
// Convert Macros to Scripts (batch)
// ---------------------------------------------------------------------------

void MainWindow::onConvertMacrosToScripts() {
    QVector<core::Macro> macros = core::MacroRecorder::loadAllMacros(macrosDir());
    if (macros.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Convert Macros",
            "No macros found to convert.");
        return;
    }

    int converted = 0;
    for (const auto &macro : macros) {
        QString scriptText = core::scripting::ScriptCompiler::macroToScript(macro);
        QString safeName = core::MacroRecorder::sanitizeFileName(macro.name);
        QString path = scriptsDir() + "/" + safeName + ".5250script";
        int suffix = 1;
        while (QFile::exists(path))
            path = scriptsDir() + "/" + safeName + "_" + QString::number(suffix++) + ".5250script";

        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(scriptText.toUtf8());
            file.close();
            ++converted;
        }
    }

    ui::widgets::StyledMessageBox::information(this, "Convert Macros",
        QString("Converted %1 of %2 macros to scripts.")
            .arg(converted).arg(macros.size()));
}
