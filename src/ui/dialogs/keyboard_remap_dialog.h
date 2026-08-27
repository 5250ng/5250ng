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
#include <QtUiStyle/BaseFramelessDialog.h>
#include <QHash>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

class QHBoxLayout;

namespace ui::dialogs {

// Small line-edit that captures a single key chord when focused.
// Emits chordCaptured() when the user presses a key; displays it as text.
class KeyChordCaptureEdit : public QLineEdit {
    Q_OBJECT
  public:
    explicit KeyChordCaptureEdit(QWidget *parent = nullptr);
    void setChord(const core::KeyChord &chord);
    core::KeyChord chord() const { return m_chord; }

  signals:
    void chordCaptured(const core::KeyChord &chord);

  protected:
    void keyPressEvent(QKeyEvent *event) override;

  private:
    core::KeyChord m_chord;
};

// Row editor showing every chord currently bound to a single action, with a
// remove button next to each and a "+ Add chord" at the end. Emits
// chordsChanged whenever the set of bound chords changes.
class ChordListEditor : public QWidget {
    Q_OBJECT
  public:
    explicit ChordListEditor(core::MappedAction action, QWidget *parent = nullptr);

    core::MappedAction action() const { return m_action; }
    QList<core::KeyChord> chords() const;

    // Replace the set of bound chords. Does not emit chordsChanged.
    void setChords(const QList<core::KeyChord> &chords);

  signals:
    void chordsChanged(core::MappedAction action, const QList<core::KeyChord> &chords);

  private:
    void addChip(const core::KeyChord &chord, bool startFocused);
    void removeChipAt(int index);
    void emitChords();

    struct Entry {
        KeyChordCaptureEdit *edit;
        QPushButton *removeBtn;
    };

    core::MappedAction m_action;
    QHBoxLayout *m_layout = nullptr;
    QPushButton *m_addBtn = nullptr;
    QList<Entry> m_entries;
};

// Dialog that lets the user edit the {chord -> action} map. Changes are only
// applied to the KeyboardMapping singleton when the user clicks "Save".
class KeyboardRemapDialog : public qt_ui_style::BaseFramelessDialog {
    Q_OBJECT
  public:
    // Optional: pre-focus on a specific action (e.g. opened via right-click on
    // the virtual keyboard). Pass MappedAction::None to skip.
    explicit KeyboardRemapDialog(QWidget *parent = nullptr,
                                 core::MappedAction focusOn = core::MappedAction::None);

  signals:
    // Emitted after the user clicks Save and the singleton has been updated.
    void mappingChanged();

  private slots:
    void onResetDefaultsClicked();
    void onImportClicked();
    void onExportClicked();
    void onSaveClicked();
    void onCancelClicked();

  private:
    void buildUI();
    void populateRows();
    int rowForAction(core::MappedAction action) const;
    ChordListEditor *editorForAction(core::MappedAction action) const;
    void onRowChordsChanged(core::MappedAction action, const QList<core::KeyChord> &chords);

    QTableWidget *m_table = nullptr;
    // Editable snapshot of bindings; flushed to the singleton on Save.
    QHash<core::KeyChord, core::MappedAction> m_working;
    core::MappedAction m_focusOn = core::MappedAction::None;
};

} // namespace ui::dialogs
