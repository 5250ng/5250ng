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

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <Qt>
#include <optional>

namespace core {

// Every 5250 action a user can bind a host key chord to.
// These are logical actions (what the operator means to do), not wire bytes.
enum class MappedAction : int {
    None = 0,
    // AID keys
    Enter,
    PF1, PF2, PF3, PF4, PF5, PF6, PF7, PF8, PF9, PF10, PF11, PF12,
    PF13, PF14, PF15, PF16, PF17, PF18, PF19, PF20, PF21, PF22, PF23, PF24,
    Clear,
    Help,
    Print,
    RollUp,
    RollDown,
    Attn,
    SysReq,
    // Field / cursor
    FieldExit,
    FieldPlus,
    FieldMinus,
    Tab,
    BackTab,
    Home,
    End,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    // Editing
    Backspace,
    Delete,
    Insert,
    EraseEOF,
    EraseField,
    EraseInput,
    Dup,
    // Misc
    Reset,
};

struct KeyChord {
    int key = 0;                                 // Qt::Key value
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;

    bool isValid() const { return key != 0; }
    bool operator==(const KeyChord &o) const {
        return key == o.key && modifiers == o.modifiers;
    }
    bool operator!=(const KeyChord &o) const { return !(*this == o); }

    // Serialize to/from a string like "Shift+F1" or "Ctrl+Alt+R".
    QString toString() const;
    static KeyChord fromString(const QString &s);
};

inline uint qHash(const KeyChord &c, uint seed = 0) noexcept {
    return ::qHash(c.key, seed) ^ static_cast<uint>(c.modifiers);
}

class KeyboardMapping {
  public:
    static KeyboardMapping &instance();

    // Persistence (uses QSettings group "KeyboardMapping").
    void load();
    void save() const;

    // Defaults mirror the hardcoded behavior of
    // KeyboardEncoder / Q5250ScreenWidget so existing users see no change.
    void resetToDefaults();

    // Lookup a chord; returns None when no binding is set.
    MappedAction lookup(int key, Qt::KeyboardModifiers modifiers) const;
    MappedAction lookup(const KeyChord &chord) const;

    // Reverse lookup: return the chord currently bound to an action, or an
    // empty chord (isValid()==false) if none is bound. When several chords are
    // bound to the same action the result is unspecified among them; callers
    // that care about the full set should use chordsFor() instead.
    KeyChord chordFor(MappedAction action) const;

    // All chords currently bound to a given action. Empty if the action has
    // no binding. Ordering is stable within a run but not guaranteed across
    // runs (QHash iteration order).
    QList<KeyChord> chordsFor(MappedAction action) const;

    // Set or clear a binding. Setting a chord that was previously bound to a
    // different action unbinds the old action silently. Setting action=None
    // removes the chord; calling clearAction() removes by action.
    void setBinding(const KeyChord &chord, MappedAction action);
    void clearChord(const KeyChord &chord);
    void clearAction(MappedAction action);

    // Full snapshot / replace (for dialog apply).
    QHash<KeyChord, MappedAction> allBindings() const { return m_chordToAction; }
    void replaceAllBindings(const QHash<KeyChord, MappedAction> &bindings);

    // JSON import/export.
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &obj);

    // Friendly names for the mapping table UI.
    static QString actionName(MappedAction action);
    static QList<MappedAction> allActions();
    static MappedAction actionFromName(const QString &name);

    // Construct an empty table. Callers typically use instance() or
    // resetToDefaults(); the public constructor is provided for JSON
    // import/export and unit tests.
    KeyboardMapping();

  private:
    QHash<KeyChord, MappedAction> m_chordToAction;
};

} // namespace core

Q_DECLARE_METATYPE(core::MappedAction)
