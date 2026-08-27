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

#include <QtUiStyle/StyledFileDialog.h>
#include <QtUiStyle/StyledMessageBox.h>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
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

// --- ChordListEditor -------------------------------------------------------

ChordListEditor::ChordListEditor(MappedAction action, QWidget *parent)
    : QWidget(parent), m_action(action) {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);
    m_addBtn = new QPushButton(tr("+ Add chord"), this);
    m_addBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_addBtn, &QPushButton::clicked, this, [this]() {
        addChip(core::KeyChord{}, /*startFocused=*/true);
    });
    m_layout->addWidget(m_addBtn);
    m_layout->addStretch(1);
}

QList<core::KeyChord> ChordListEditor::chords() const {
    QList<core::KeyChord> out;
    for (const auto &e : m_entries) {
        core::KeyChord c = e.edit->chord();
        if (c.isValid()) out.append(c);
    }
    return out;
}

void ChordListEditor::setChords(const QList<core::KeyChord> &chords) {
    // Clear and rebuild; block signals so we don't emit during setup.
    while (!m_entries.isEmpty()) {
        Entry e = m_entries.takeLast();
        m_layout->removeWidget(e.edit);
        m_layout->removeWidget(e.removeBtn);
        e.edit->deleteLater();
        e.removeBtn->deleteLater();
    }
    QSignalBlocker block(this);
    for (const auto &c : chords) addChip(c, /*startFocused=*/false);
}

void ChordListEditor::addChip(const core::KeyChord &chord, bool startFocused) {
    Entry entry;
    entry.edit = new KeyChordCaptureEdit(this);
    entry.edit->setChord(chord);
    entry.edit->setMinimumWidth(140);
    entry.removeBtn = new QPushButton(QString::fromUtf8("\xc3\x97"), this); // ×
    entry.removeBtn->setFocusPolicy(Qt::NoFocus);
    entry.removeBtn->setFixedWidth(24);
    entry.removeBtn->setToolTip(tr("Remove this chord"));

    // Insert before the "+ Add chord" button and the trailing stretch.
    int insertAt = m_layout->count() - 2;
    if (insertAt < 0) insertAt = 0;
    m_layout->insertWidget(insertAt, entry.edit);
    m_layout->insertWidget(insertAt + 1, entry.removeBtn);
    m_entries.append(entry);

    connect(entry.edit, &KeyChordCaptureEdit::chordCaptured, this,
            [this](const core::KeyChord &) { emitChords(); });
    connect(entry.removeBtn, &QPushButton::clicked, this, [this, editPtr = entry.edit]() {
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].edit == editPtr) { removeChipAt(i); return; }
        }
    });

    if (startFocused) entry.edit->setFocus();
}

void ChordListEditor::removeChipAt(int index) {
    if (index < 0 || index >= m_entries.size()) return;
    Entry e = m_entries.takeAt(index);
    m_layout->removeWidget(e.edit);
    m_layout->removeWidget(e.removeBtn);
    e.edit->deleteLater();
    e.removeBtn->deleteLater();
    emitChords();
}

void ChordListEditor::emitChords() {
    emit chordsChanged(m_action, chords());
}

// --- KeyboardRemapDialog ---------------------------------------------------

KeyboardRemapDialog::KeyboardRemapDialog(QWidget *parent, MappedAction focusOn)
    : qt_ui_style::BaseFramelessDialog(parent), m_focusOn(focusOn) {
    setWindowTitle(tr("Remap Keyboard"));
    resize(720, 560);
    m_working = KeyboardMapping::instance().allBindings();
    buildUI();
    populateRows();
    if (m_focusOn != MappedAction::None) {
        int row = rowForAction(m_focusOn);
        if (row >= 0) {
            m_table->scrollToItem(m_table->item(row, 0));
            m_table->setCurrentCell(row, 1);
            ChordListEditor *editor = editorForAction(m_focusOn);
            if (editor) editor->setFocus();
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
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("Action"), tr("Chords")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
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

        // Collect every chord currently bound to this action from the working copy.
        QList<KeyChord> bound;
        for (auto it = m_working.constBegin(); it != m_working.constEnd(); ++it) {
            if (it.value() == action) bound.append(it.key());
        }

        auto *editor = new ChordListEditor(action, this);
        editor->setChords(bound);
        connect(editor, &ChordListEditor::chordsChanged,
                this, &KeyboardRemapDialog::onRowChordsChanged);
        m_table->setCellWidget(r, 1, editor);
    }
    m_table->resizeRowsToContents();
}

void KeyboardRemapDialog::onRowChordsChanged(MappedAction action,
                                             const QList<KeyChord> &chords) {
    // Snapshot this action's previous bindings BEFORE any mutation so we can
    // roll the editor back if the user declines a conflict prompt.
    QList<KeyChord> previous;
    for (auto it = m_working.constBegin(); it != m_working.constEnd(); ++it) {
        if (it.value() == action) previous.append(it.key());
    }

    // Ask the user before silently stealing a chord from a different action.
    // Only newly captured chords are candidates — reordering or re-entering the
    // same chord must not trigger a prompt.
    for (const auto &chord : chords) {
        if (!chord.isValid()) continue;
        if (previous.contains(chord)) continue;
        auto existing = m_working.constFind(chord);
        if (existing == m_working.constEnd() || existing.value() == action) continue;

        MappedAction prevOwner = existing.value();
        QString prompt = tr(
            "%1 is currently bound to \"%2\".\n\n"
            "Replace that binding with \"%3\"?")
            .arg(chord.toString(),
                 KeyboardMapping::actionName(prevOwner),
                 KeyboardMapping::actionName(action));
        auto result = qt_ui_style::StyledMessageBox::question(
            this, tr("Replace existing binding?"), prompt);
        if (result != qt_ui_style::StyledMessageBox::Yes) {
            // Roll the row back to its prior state without touching m_working.
            ChordListEditor *editor = editorForAction(action);
            if (editor) {
                QSignalBlocker block(editor);
                editor->setChords(previous);
            }
            m_table->resizeRowsToContents();
            return;
        }
    }

    // Drop every existing binding for this action so we can rebuild from the
    // editor's current set.
    for (auto it = m_working.begin(); it != m_working.end();) {
        if (it.value() == action) it = m_working.erase(it);
        else ++it;
    }

    // For each captured chord: steal it from whichever action owned it, then
    // assign it to this one. Editors that lose a chord are refreshed in place.
    for (const auto &chord : chords) {
        if (!chord.isValid()) continue;
        auto existing = m_working.constFind(chord);
        if (existing != m_working.constEnd() && existing.value() != action) {
            MappedAction prevOwner = existing.value();
            m_working.remove(chord);
            ChordListEditor *otherEditor = editorForAction(prevOwner);
            if (otherEditor) {
                QList<KeyChord> remaining;
                for (const auto &c : otherEditor->chords()) {
                    if (c != chord) remaining.append(c);
                }
                QSignalBlocker block(otherEditor);
                otherEditor->setChords(remaining);
            }
        }
        m_working.insert(chord, action);
    }
    m_table->resizeRowsToContents();
}

int KeyboardRemapDialog::rowForAction(MappedAction action) const {
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *it = m_table->item(r, 0);
        if (it && it->data(Qt::UserRole).toInt() == static_cast<int>(action)) return r;
    }
    return -1;
}

ChordListEditor *KeyboardRemapDialog::editorForAction(MappedAction action) const {
    int row = rowForAction(action);
    if (row < 0) return nullptr;
    return qobject_cast<ChordListEditor *>(m_table->cellWidget(row, 1));
}

void KeyboardRemapDialog::onResetDefaultsClicked() {
    KeyboardMapping tmp;
    tmp.resetToDefaults();
    m_working = tmp.allBindings();
    m_table->clearContents();
    populateRows();
}

void KeyboardRemapDialog::onImportClicked() {
    QString path = qt_ui_style::StyledFileDialog::getOpenFileName(
        this, tr("Import Keyboard Mapping"), QString(), tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Import failed"),
            tr("Could not open %1.").arg(path));
        return;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Import failed"),
            tr("Invalid JSON: %1").arg(err.errorString()));
        return;
    }
    KeyboardMapping tmp;
    tmp.resetToDefaults();
    if (!tmp.fromJson(doc.object())) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Import failed"),
            tr("File does not contain a keyboard mapping."));
        return;
    }
    m_working = tmp.allBindings();
    m_table->clearContents();
    populateRows();
}

void KeyboardRemapDialog::onExportClicked() {
    QString path = qt_ui_style::StyledFileDialog::getSaveFileName(
        this, tr("Export Keyboard Mapping"), QString(), tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    KeyboardMapping tmp;
    tmp.replaceAllBindings(m_working);
    QJsonDocument doc(tmp.toJson());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Export failed"),
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
