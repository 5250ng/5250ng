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

#include "keyboard_remap_dialog.h"

#include "ui/widgets/Frameless/StyledFileDialog.h"
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

namespace ui::dialogs {

using core::KeyboardMapping;
using core::KeyChord;
using core::MappedAction;

// --- KeyChordCaptureEdit ----------------------------------------------------

KeyChordCaptureEdit::KeyChordCaptureEdit(QWidget *parent) : QLineEdit(parent) {
    setReadOnly(true);
    setPlaceholderText(tr("Click and press a chord…"));
    setFocusPolicy(Qt::StrongFocus);
}

void KeyChordCaptureEdit::setChord(const core::KeyChord &chord) {
    m_chord = chord;
    setText(chord.isValid() ? chord.toString() : QString());
}

void KeyChordCaptureEdit::keyPressEvent(QKeyEvent *event) {
    // Ignore bare modifier presses — wait for a real key.
    int key = event->key();
    if (key == Qt::Key_Shift || key == Qt::Key_Control ||
        key == Qt::Key_Alt || key == Qt::Key_Meta ||
        key == Qt::Key_AltGr || key == Qt::Key_CapsLock) {
        QLineEdit::keyPressEvent(event);
        return;
    }
    KeyChord c;
    c.key = key;
    c.modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier |
                                        Qt::AltModifier | Qt::MetaModifier);
    setChord(c);
    emit chordCaptured(c);
    event->accept();
}

// --- KeyboardRemapDialog ---------------------------------------------------

KeyboardRemapDialog::KeyboardRemapDialog(QWidget *parent, MappedAction focusOn)
    : ui::widgets::BaseFramelessDialog(parent), m_focusOn(focusOn) {
    setWindowTitle(tr("Remap Keyboard"));
    resize(640, 520);
    m_working = KeyboardMapping::instance().allBindings();
    buildUI();
    populateRows();
    if (m_focusOn != MappedAction::None) {
        int row = rowForAction(m_focusOn);
        if (row >= 0) {
            m_table->scrollToItem(m_table->item(row, 0));
            m_table->setCurrentCell(row, 1);
            QWidget *w = m_table->cellWidget(row, 1);
            if (w) w->setFocus();
        }
    }
}

void KeyboardRemapDialog::buildUI() {
    QVBoxLayout *root = contentLayout();
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    QLabel *hint = new QLabel(
        tr("Click a Chord field, then press the host keyboard combination you "
           "want bound to the 5250 action. Leave blank to unbind."), this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Action"), tr("Chord"), tr("")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *resetBtn = new QPushButton(tr("Reset to Defaults"), this);
    QPushButton *importBtn = new QPushButton(tr("Import…"), this);
    QPushButton *exportBtn = new QPushButton(tr("Export…"), this);
    btns->addWidget(resetBtn);
    btns->addWidget(importBtn);
    btns->addWidget(exportBtn);
    btns->addStretch(1);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    QPushButton *saveBtn = new QPushButton(tr("Save"), this);
    saveBtn->setDefault(true);
    btns->addWidget(cancelBtn);
    btns->addWidget(saveBtn);
    root->addLayout(btns);

    connect(resetBtn, &QPushButton::clicked, this, &KeyboardRemapDialog::onResetDefaultsClicked);
    connect(importBtn, &QPushButton::clicked, this, &KeyboardRemapDialog::onImportClicked);
    connect(exportBtn, &QPushButton::clicked, this, &KeyboardRemapDialog::onExportClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &KeyboardRemapDialog::onCancelClicked);
    connect(saveBtn, &QPushButton::clicked, this, &KeyboardRemapDialog::onSaveClicked);
}

void KeyboardRemapDialog::populateRows() {
    const QList<MappedAction> actions = KeyboardMapping::allActions();
    m_table->setRowCount(actions.size());
    for (int r = 0; r < actions.size(); ++r) {
        MappedAction action = actions[r];
        auto *nameItem = new QTableWidgetItem(KeyboardMapping::actionName(action));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, static_cast<int>(action));
        m_table->setItem(r, 0, nameItem);

        // Find the chord currently bound to this action in the working copy.
        KeyChord bound;
        for (auto it = m_working.constBegin(); it != m_working.constEnd(); ++it) {
            if (it.value() == action) { bound = it.key(); break; }
        }

        auto *chordEdit = new KeyChordCaptureEdit(this);
        chordEdit->setChord(bound);
        connect(chordEdit, &KeyChordCaptureEdit::chordCaptured, this,
                [this, action, chordEdit](const KeyChord &chord) {
                    // Remove the action's old binding and any other action that used the new chord.
                    for (auto it = m_working.begin(); it != m_working.end();) {
                        if (it.value() == action) {
                            it = m_working.erase(it);
                        } else if (chord.isValid() && it.key() == chord) {
                            // Clear the row that previously owned this chord.
                            MappedAction replaced = it.value();
                            int row = rowForAction(replaced);
                            if (row >= 0) {
                                auto *w = qobject_cast<KeyChordCaptureEdit *>(m_table->cellWidget(row, 1));
                                if (w) w->setChord(KeyChord{});
                            }
                            it = m_working.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    if (chord.isValid()) m_working.insert(chord, action);
                    // Reflect change in the edit itself.
                    chordEdit->setChord(chord);
                });
        m_table->setCellWidget(r, 1, chordEdit);

        auto *clearBtn = new QPushButton(tr("Clear"), this);
        clearBtn->setFocusPolicy(Qt::NoFocus);
        connect(clearBtn, &QPushButton::clicked, this, [this, action, chordEdit]() {
            for (auto it = m_working.begin(); it != m_working.end();) {
                if (it.value() == action) it = m_working.erase(it);
                else ++it;
            }
            chordEdit->setChord(KeyChord{});
        });
        m_table->setCellWidget(r, 2, clearBtn);
    }
}

int KeyboardRemapDialog::rowForAction(MappedAction action) const {
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *it = m_table->item(r, 0);
        if (it && it->data(Qt::UserRole).toInt() == static_cast<int>(action)) return r;
    }
    return -1;
}

void KeyboardRemapDialog::onResetDefaultsClicked() {
    KeyboardMapping tmp;
    tmp.resetToDefaults();
    m_working = tmp.allBindings();
    m_table->clearContents();
    populateRows();
}

void KeyboardRemapDialog::onImportClicked() {
    QString path = ui::widgets::StyledFileDialog::getOpenFileName(
        this, tr("Import Keyboard Mapping"), QString(), tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        ui::widgets::StyledMessageBox::warning(this, tr("Import failed"),
            tr("Could not open %1.").arg(path));
        return;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        ui::widgets::StyledMessageBox::warning(this, tr("Import failed"),
            tr("Invalid JSON: %1").arg(err.errorString()));
        return;
    }
    KeyboardMapping tmp;
    tmp.resetToDefaults();
    if (!tmp.fromJson(doc.object())) {
        ui::widgets::StyledMessageBox::warning(this, tr("Import failed"),
            tr("File does not contain a keyboard mapping."));
        return;
    }
    m_working = tmp.allBindings();
    m_table->clearContents();
    populateRows();
}

void KeyboardRemapDialog::onExportClicked() {
    QString path = ui::widgets::StyledFileDialog::getSaveFileName(
        this, tr("Export Keyboard Mapping"), QString(), tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    KeyboardMapping tmp;
    tmp.replaceAllBindings(m_working);
    QJsonDocument doc(tmp.toJson());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ui::widgets::StyledMessageBox::warning(this, tr("Export failed"),
            tr("Could not write %1.").arg(path));
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
}

void KeyboardRemapDialog::onSaveClicked() {
    KeyboardMapping::instance().replaceAllBindings(m_working);
    KeyboardMapping::instance().save();
    emit mappingChanged();
    accept();
}

void KeyboardRemapDialog::onCancelClicked() {
    reject();
}

} // namespace ui::dialogs
