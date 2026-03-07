#include "../main_window.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QApplication>
#include <QDir>
#include <QPointer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>

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
        auto *cancelBtn = new QPushButton("Cancel", dlg);
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
            QString path = macrosDir() + "/" + name + ".json";
            if (core::MacroRecorder::saveMacro(macro, path)) {
                ui::widgets::StyledMessageBox::information(this, "Macro Saved",
                    QString("Macro '%1' saved with %2 steps.")
                        .arg(name).arg(macro.steps.size()));
            }
        }
        m_macroRecordAction->setChecked(false);
    } else {
        s->macroRecorder->startRecording();
        m_macroRecordAction->setChecked(true);
    }
}

void MainWindow::onPlayMacro() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->macroRecorder) {
        s->macroRecorder = new core::MacroRecorder(s->container);
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
    dlg->setFixedSize(400, 320);

    auto *list = new QListWidget(dlg);
    for (const auto &m : allMacros) {
        list->addItem(QString("%1  (%2 steps)").arg(m.name).arg(m.steps.size()));
    }
    if (list->count() > 0) list->setCurrentRow(0);

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
    dlg->contentLayout()->addLayout(btnLayout);

    connect(playBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, dlg, &QDialog::accept);

    int result = dlg->exec();
    int idx = list->currentRow();
    delete dlg;

    if (result != QDialog::Accepted) return;
    if (idx < 0 || idx >= allMacros.size()) return;

    core::Macro macro = allMacros[idx];

    // Connect async playback — recorder emits steps one at a time with delays
    // Use QPointer to guard against session being closed during playback
    QPointer<ui::widgets::Q5250ScreenWidget> displayGuard = s->displayWidget;
    auto conn = std::make_shared<QMetaObject::Connection>();
    auto finishConn = std::make_shared<QMetaObject::Connection>();

    *conn = connect(s->macroRecorder, &core::MacroRecorder::playbackStep, this,
        [displayGuard](const core::MacroStep &step) {
            if (displayGuard.isNull()) return;
            if (step.type == core::MacroStep::KeyPress) {
                QKeyEvent ev(QEvent::KeyPress, step.key, step.mods, step.text);
                QApplication::sendEvent(displayGuard.data(), &ev);
            } else if (step.type == core::MacroStep::AIDKey) {
                QByteArray aid;
                aid.append(static_cast<char>(step.aidByte));
                displayGuard->processEncodedInput(aid, true);
            }
        });

    *finishConn = connect(s->macroRecorder, &core::MacroRecorder::playbackFinished, this,
        [conn, finishConn]() {
            QObject::disconnect(*conn);
            QObject::disconnect(*finishConn);
        });

    s->macroRecorder->play(macro);
}

void MainWindow::onManageMacros() {
    QString dir = macrosDir();

    QVector<core::Macro> macros = core::MacroRecorder::loadAllMacros(dir);
    if (macros.isEmpty()) {
        ui::widgets::StyledMessageBox::information(this, "Macros",
            "No saved macros found.\n\nRecord a macro using Macros > Record/Stop.");
        return;
    }

    // Frameless dialog with list of macros and delete button
    auto *dlg = new ui::widgets::BaseFramelessDialog(this);
    dlg->setWindowTitle("Manage Macros");
    dlg->setFixedSize(450, 360);

    auto *list = new QListWidget(dlg);
    QDir macroDir(dir);
    QStringList files = macroDir.entryList({"*.json", "*.macro"}, QDir::Files);
    for (const auto &m : macros) {
        auto *item = new QListWidgetItem(
            QString("%1  (%2 steps)").arg(m.name).arg(m.steps.size()));
        list->addItem(item);
    }

    auto *pathLabel = new QLabel(QString("Macros stored in: %1").arg(dir), dlg);
    pathLabel->setWordWrap(true);
    pathLabel->setStyleSheet("color: gray; font-size: 10px;");

    auto *btnLayout = new QHBoxLayout();
    auto *deleteBtn = new QPushButton("Delete", dlg);
    auto *closeBtn = new QPushButton("Close", dlg);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);

    dlg->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dlg->contentLayout()->setSpacing(8);
    dlg->contentLayout()->addWidget(new QLabel("Saved macros:", dlg));
    dlg->contentLayout()->addWidget(list, 1);
    dlg->contentLayout()->addWidget(pathLabel);
    dlg->contentLayout()->addLayout(btnLayout);

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(deleteBtn, &QPushButton::clicked, dlg, [list, &macros, &files, dir]() {
        int idx = list->currentRow();
        if (idx < 0 || idx >= files.size()) return;
        QString filePath = dir + "/" + files[idx];
        QFile::remove(filePath);
        delete list->takeItem(idx);
        files.removeAt(idx);
        macros.removeAt(idx);
    });

    dlg->exec();
    delete dlg;
}
