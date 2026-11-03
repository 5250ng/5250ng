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

#include "macro_to_script.h"
#include "core/keyboard_mapping.h"
#include "core/macro_config.h"
#include <5250script/script_compiler.h>

namespace core {

namespace {

// The script statement that replays a local (non-AID) keyboard action, or an
// empty string for AID actions and None.
QString scriptLineForAction(MappedAction action) {
    switch (action) {
    case MappedAction::FieldExit:  return QStringLiteral("PRESS FIELDEXIT");
    case MappedAction::FieldPlus:  return QStringLiteral("PRESS FIELDPLUS");
    case MappedAction::FieldMinus: return QStringLiteral("PRESS FIELDMINUS");
    case MappedAction::Tab:        return QStringLiteral("MOVE CURSOR AT NEXT INPUTFIELD");
    case MappedAction::BackTab:    return QStringLiteral("MOVE CURSOR AT PREVIOUS INPUTFIELD");
    case MappedAction::Home:       return QStringLiteral("PRESS HOME");
    case MappedAction::End:        return QStringLiteral("PRESS END");
    case MappedAction::ArrowUp:    return QStringLiteral("MOVE CURSOR UP 1");
    case MappedAction::ArrowDown:  return QStringLiteral("MOVE CURSOR DOWN 1");
    case MappedAction::ArrowLeft:  return QStringLiteral("MOVE CURSOR LEFT 1");
    case MappedAction::ArrowRight: return QStringLiteral("MOVE CURSOR RIGHT 1");
    case MappedAction::Backspace:  return QStringLiteral("PRESS BACKSPACE");
    case MappedAction::Delete:     return QStringLiteral("PRESS DELETE");
    case MappedAction::Insert:     return QStringLiteral("PRESS INSERT");
    case MappedAction::EraseEOF:   return QStringLiteral("PRESS ERASEEOF");
    case MappedAction::EraseField: return QStringLiteral("PRESS ERASEFIELD");
    case MappedAction::EraseInput: return QStringLiteral("PRESS ERASEINPUT");
    case MappedAction::Dup:        return QStringLiteral("PRESS DUP");
    case MappedAction::Reset:      return QStringLiteral("PRESS ESC");
    default:                       return QString();
    }
}

} // namespace

QString macroToScript(const Macro &macro) {
    QStringList lines;

    using core::scripting::ScriptCompiler;
    lines << "# @script.name = " + ScriptCompiler::quoteMetadataValue(macro.name);
    if (!macro.description.isEmpty())
        lines << "# @script.description = " + ScriptCompiler::quoteMetadataValue(macro.description);
    lines << QString("# @script.version = \"1.0\"");
    lines << "";

    core::MacroConfig::instance().load();
    bool recordTimings = core::MacroConfig::instance().recordTimings();

    // Coalesce consecutive KeyPress steps into TYPE lines
    QString pendingString;

    auto flushString = [&]() {
        if (!pendingString.isEmpty()) {
            QString escaped = pendingString;
            escaped.replace('\\', "\\\\");
            escaped.replace('"', "\\\"");
            // TYPE interpolates $NAME; $$ types a literal $
            escaped.replace('$', "$$");
            lines << QString("TYPE \"%1\"").arg(escaped);
            pendingString.clear();
        }
    };

    for (const auto &step : macro.steps) {
        switch (step.type) {
        case MacroStep::KeyPress: {
            // Resolve the chord as the terminal did when it was recorded:
            // Ctrl+End is Field Exit, Shift+Insert is Dup, ... - not the bare
            // key. Chords that sent an AID (Enter, PF keys, Roll, Attn,
            // SysReq, ...) are written from the AID step that follows.
            const Qt::KeyboardModifiers mods = step.mods
                & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
            const MappedAction action = KeyboardMapping::instance().lookup(step.key, mods);
            const QString line = scriptLineForAction(action);
            if (!line.isEmpty()) {
                flushString();
                lines << line;
            } else if (action == MappedAction::None && !step.text.isEmpty()
                       && step.text.at(0).isPrint()) {
                pendingString += step.text;
            } else {
                // An AID chord (its AID step follows) or a key that did
                // nothing on the terminal
                flushString();
            }
            break;
        }

        case MacroStep::AIDKey: {
            flushString();
            QString keyword = scripting::ScriptCompiler::aidByteToKeyword(step.aidByte);
            if (keyword.startsWith('#')) {
                // Unknown AID byte — emit as a comment, not as a PRESS command
                lines << keyword;
            } else {
                lines << QString("PRESS %1").arg(keyword);
            }
            break;
        }

        case MacroStep::Delay:
            if (recordTimings) {
                flushString();
                if (step.delayMs > 0)
                    lines << QString("WAIT %1").arg(step.delayMs);
            }
            break;
        }
    }

    flushString();
    return lines.join("\n") + "\n";
}

} // namespace core
