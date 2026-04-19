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

#include "keyboard_mapping.h"

#include <QJsonArray>
#include <QKeySequence>
#include <QSettings>
#include <QStringList>

namespace core {

namespace {

// Action <-> name mapping used for JSON and the dialog.
// The name is stable (not localized) so saved mappings survive UI changes.
struct ActionEntry {
    MappedAction action;
    const char *name;
};

const ActionEntry kActionEntries[] = {
    {MappedAction::None, "None"},
    {MappedAction::Enter, "Enter"},
    {MappedAction::PF1, "PF1"},   {MappedAction::PF2, "PF2"},   {MappedAction::PF3, "PF3"},
    {MappedAction::PF4, "PF4"},   {MappedAction::PF5, "PF5"},   {MappedAction::PF6, "PF6"},
    {MappedAction::PF7, "PF7"},   {MappedAction::PF8, "PF8"},   {MappedAction::PF9, "PF9"},
    {MappedAction::PF10, "PF10"}, {MappedAction::PF11, "PF11"}, {MappedAction::PF12, "PF12"},
    {MappedAction::PF13, "PF13"}, {MappedAction::PF14, "PF14"}, {MappedAction::PF15, "PF15"},
    {MappedAction::PF16, "PF16"}, {MappedAction::PF17, "PF17"}, {MappedAction::PF18, "PF18"},
    {MappedAction::PF19, "PF19"}, {MappedAction::PF20, "PF20"}, {MappedAction::PF21, "PF21"},
    {MappedAction::PF22, "PF22"}, {MappedAction::PF23, "PF23"}, {MappedAction::PF24, "PF24"},
    {MappedAction::Clear, "Clear"},
    {MappedAction::Help, "Help"},
    {MappedAction::Print, "Print"},
    {MappedAction::RollUp, "RollUp"},
    {MappedAction::RollDown, "RollDown"},
    {MappedAction::Attn, "Attn"},
    {MappedAction::SysReq, "SysReq"},
    {MappedAction::FieldExit, "FieldExit"},
    {MappedAction::FieldPlus, "FieldPlus"},
    {MappedAction::FieldMinus, "FieldMinus"},
    {MappedAction::Tab, "Tab"},
    {MappedAction::BackTab, "BackTab"},
    {MappedAction::Home, "Home"},
    {MappedAction::End, "End"},
    {MappedAction::ArrowUp, "ArrowUp"},
    {MappedAction::ArrowDown, "ArrowDown"},
    {MappedAction::ArrowLeft, "ArrowLeft"},
    {MappedAction::ArrowRight, "ArrowRight"},
    {MappedAction::Backspace, "Backspace"},
    {MappedAction::Delete, "Delete"},
    {MappedAction::Insert, "Insert"},
    {MappedAction::EraseEOF, "EraseEOF"},
    {MappedAction::EraseField, "EraseField"},
    {MappedAction::EraseInput, "EraseInput"},
    {MappedAction::Dup, "Dup"},
    {MappedAction::Reset, "Reset"},
};

} // namespace

QString KeyChord::toString() const {
    if (!isValid()) return QString();
    return QKeySequence(static_cast<int>(modifiers) | key).toString(QKeySequence::PortableText);
}

KeyChord KeyChord::fromString(const QString &s) {
    KeyChord out;
    if (s.isEmpty()) return out;
    QKeySequence seq(s, QKeySequence::PortableText);
    if (seq.isEmpty()) return out;
    int combined = seq[0].toCombined();
    out.key = combined & ~Qt::KeyboardModifierMask;
    out.modifiers = QFlags<Qt::KeyboardModifier>(
        static_cast<Qt::KeyboardModifiers>(combined & Qt::KeyboardModifierMask));
    return out;
}

KeyboardMapping::KeyboardMapping() {
    resetToDefaults();
}

KeyboardMapping &KeyboardMapping::instance() {
    static KeyboardMapping inst;
    return inst;
}

void KeyboardMapping::resetToDefaults() {
    m_chordToAction.clear();

    // PF1..PF12 → F1..F12
    for (int i = 0; i < 12; ++i) {
        KeyChord c{Qt::Key_F1 + i, Qt::NoModifier};
        m_chordToAction.insert(c, static_cast<MappedAction>(
            static_cast<int>(MappedAction::PF1) + i));
    }
    // PF13..PF24 → Shift+F1..Shift+F12 (matches the existing convention)
    for (int i = 0; i < 12; ++i) {
        KeyChord c{Qt::Key_F1 + i, Qt::ShiftModifier};
        m_chordToAction.insert(c, static_cast<MappedAction>(
            static_cast<int>(MappedAction::PF13) + i));
    }
    // Native Qt F13..F24 when available on the host keyboard
    for (int i = 0; i < 12; ++i) {
        KeyChord c{Qt::Key_F13 + i, Qt::NoModifier};
        m_chordToAction.insert(c, static_cast<MappedAction>(
            static_cast<int>(MappedAction::PF13) + i));
    }

    m_chordToAction.insert({Qt::Key_Return, Qt::NoModifier}, MappedAction::Enter);
    m_chordToAction.insert({Qt::Key_Enter, Qt::NoModifier}, MappedAction::Enter);
    m_chordToAction.insert({Qt::Key_Escape, Qt::ControlModifier}, MappedAction::Attn);
    m_chordToAction.insert({Qt::Key_SysReq, Qt::NoModifier}, MappedAction::SysReq);
    m_chordToAction.insert({Qt::Key_PageUp, Qt::NoModifier}, MappedAction::RollUp);
    m_chordToAction.insert({Qt::Key_PageDown, Qt::NoModifier}, MappedAction::RollDown);
    m_chordToAction.insert({Qt::Key_Tab, Qt::NoModifier}, MappedAction::Tab);
    m_chordToAction.insert({Qt::Key_Backtab, Qt::NoModifier}, MappedAction::BackTab);
    m_chordToAction.insert({Qt::Key_Tab, Qt::ShiftModifier}, MappedAction::BackTab);
    m_chordToAction.insert({Qt::Key_Home, Qt::NoModifier}, MappedAction::Home);
    m_chordToAction.insert({Qt::Key_End, Qt::NoModifier}, MappedAction::End);
    m_chordToAction.insert({Qt::Key_End, Qt::ControlModifier}, MappedAction::FieldExit);
    m_chordToAction.insert({Qt::Key_Up, Qt::NoModifier}, MappedAction::ArrowUp);
    m_chordToAction.insert({Qt::Key_Down, Qt::NoModifier}, MappedAction::ArrowDown);
    m_chordToAction.insert({Qt::Key_Left, Qt::NoModifier}, MappedAction::ArrowLeft);
    m_chordToAction.insert({Qt::Key_Right, Qt::NoModifier}, MappedAction::ArrowRight);
    m_chordToAction.insert({Qt::Key_Backspace, Qt::NoModifier}, MappedAction::Backspace);
    m_chordToAction.insert({Qt::Key_Backspace, Qt::ControlModifier}, MappedAction::EraseField);
    m_chordToAction.insert({Qt::Key_Delete, Qt::NoModifier}, MappedAction::Delete);
    m_chordToAction.insert({Qt::Key_Delete, Qt::ControlModifier}, MappedAction::EraseEOF);
    m_chordToAction.insert({Qt::Key_Delete, Qt::AltModifier}, MappedAction::EraseInput);
    m_chordToAction.insert({Qt::Key_Insert, Qt::NoModifier}, MappedAction::Insert);
    m_chordToAction.insert({Qt::Key_Insert, Qt::ShiftModifier}, MappedAction::Dup);
    m_chordToAction.insert({Qt::Key_Plus, Qt::AltModifier}, MappedAction::FieldPlus);
    m_chordToAction.insert({Qt::Key_Minus, Qt::AltModifier}, MappedAction::FieldMinus);
}

MappedAction KeyboardMapping::lookup(int key, Qt::KeyboardModifiers modifiers) const {
    return lookup(KeyChord{key, modifiers});
}

MappedAction KeyboardMapping::lookup(const KeyChord &chord) const {
    auto it = m_chordToAction.constFind(chord);
    if (it == m_chordToAction.constEnd()) return MappedAction::None;
    return it.value();
}

KeyChord KeyboardMapping::chordFor(MappedAction action) const {
    for (auto it = m_chordToAction.constBegin(); it != m_chordToAction.constEnd(); ++it) {
        if (it.value() == action) return it.key();
    }
    return KeyChord{};
}

void KeyboardMapping::setBinding(const KeyChord &chord, MappedAction action) {
    if (!chord.isValid()) return;
    if (action == MappedAction::None) {
        m_chordToAction.remove(chord);
        return;
    }
    m_chordToAction.insert(chord, action);
}

void KeyboardMapping::clearChord(const KeyChord &chord) {
    m_chordToAction.remove(chord);
}

void KeyboardMapping::clearAction(MappedAction action) {
    QList<KeyChord> toRemove;
    for (auto it = m_chordToAction.constBegin(); it != m_chordToAction.constEnd(); ++it) {
        if (it.value() == action) toRemove.append(it.key());
    }
    for (const auto &c : toRemove) m_chordToAction.remove(c);
}

void KeyboardMapping::replaceAllBindings(const QHash<KeyChord, MappedAction> &bindings) {
    m_chordToAction = bindings;
}

void KeyboardMapping::load() {
    QSettings settings;
    settings.beginGroup("KeyboardMapping");
    // beginReadArray returns 0 when nothing was saved, which preserves defaults.
    int n = settings.beginReadArray("entries");
    if (n <= 0) {
        settings.endArray();
        settings.endGroup();
        return;
    }
    QHash<KeyChord, MappedAction> loaded;
    for (int i = 0; i < n; ++i) {
        settings.setArrayIndex(i);
        QString chordStr = settings.value("chord").toString();
        QString actionName = settings.value("action").toString();
        KeyChord chord = KeyChord::fromString(chordStr);
        MappedAction action = KeyboardMapping::actionFromName(actionName);
        if (chord.isValid() && action != MappedAction::None) {
            loaded.insert(chord, action);
        }
    }
    settings.endArray();
    settings.endGroup();
    // Replace unconditionally: a saved mapping with zero valid entries is an
    // explicit "no bindings" state the user intentionally exported.
    m_chordToAction = loaded;
}

void KeyboardMapping::save() const {
    QSettings settings;
    settings.beginGroup("KeyboardMapping");
    settings.remove("");
    settings.beginWriteArray("entries", m_chordToAction.size());
    int i = 0;
    for (auto it = m_chordToAction.constBegin(); it != m_chordToAction.constEnd(); ++it, ++i) {
        settings.setArrayIndex(i);
        settings.setValue("chord", it.key().toString());
        settings.setValue("action", KeyboardMapping::actionName(it.value()));
    }
    settings.endArray();
    settings.endGroup();
}

QJsonObject KeyboardMapping::toJson() const {
    QJsonArray arr;
    for (auto it = m_chordToAction.constBegin(); it != m_chordToAction.constEnd(); ++it) {
        QJsonObject o;
        o["chord"] = it.key().toString();
        o["action"] = KeyboardMapping::actionName(it.value());
        arr.append(o);
    }
    QJsonObject root;
    root["entries"] = arr;
    return root;
}

bool KeyboardMapping::fromJson(const QJsonObject &obj) {
    if (!obj.contains("entries") || !obj["entries"].isArray()) return false;
    QHash<KeyChord, MappedAction> loaded;
    for (const auto &v : obj["entries"].toArray()) {
        if (!v.isObject()) continue;
        QJsonObject e = v.toObject();
        KeyChord chord = KeyChord::fromString(e["chord"].toString());
        MappedAction action = KeyboardMapping::actionFromName(e["action"].toString());
        if (chord.isValid() && action != MappedAction::None) {
            loaded.insert(chord, action);
        }
    }
    m_chordToAction = loaded;
    return true;
}

QString KeyboardMapping::actionName(MappedAction action) {
    for (const auto &e : kActionEntries) {
        if (e.action == action) return QString::fromLatin1(e.name);
    }
    return QStringLiteral("None");
}

MappedAction KeyboardMapping::actionFromName(const QString &name) {
    for (const auto &e : kActionEntries) {
        if (name == QLatin1String(e.name)) return e.action;
    }
    return MappedAction::None;
}

QList<MappedAction> KeyboardMapping::allActions() {
    QList<MappedAction> out;
    for (const auto &e : kActionEntries) {
        if (e.action != MappedAction::None) out.append(e.action);
    }
    return out;
}

} // namespace core
