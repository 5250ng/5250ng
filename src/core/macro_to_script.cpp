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
#include "core/macro_config.h"
#include <5250script/script_compiler.h>

namespace core {

QString macroToScript(const Macro &macro) {
    QStringList lines;

    lines << QString("# @script.name = \"%1\"").arg(macro.name);
    if (!macro.description.isEmpty())
        lines << QString("# @script.description = \"%1\"").arg(macro.description);
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
            lines << QString("TYPE \"%1\"").arg(escaped);
            pendingString.clear();
        }
    };

    for (const auto &step : macro.steps) {
        switch (step.type) {
        case MacroStep::KeyPress: {
            // Check for special keys first — these may have non-empty text
            // (e.g. Tab produces "\t", Backspace produces "\b") but must be
            // emitted as commands, not coalesced into TYPE strings.
            bool handled = true;
            switch (step.key) {
            case Qt::Key_Tab:
                flushString();
                if (step.mods & Qt::ShiftModifier)
                    lines << "MOVE CURSOR AT PREVIOUS INPUTFIELD";
                else
                    lines << "MOVE CURSOR AT NEXT INPUTFIELD";
                break;
            case Qt::Key_Backtab:   flushString(); lines << "MOVE CURSOR AT PREVIOUS INPUTFIELD"; break;
            case Qt::Key_Backspace: flushString(); lines << "PRESS BACKSPACE"; break;
            case Qt::Key_Delete:    flushString(); lines << "PRESS DELETE"; break;
            case Qt::Key_Insert:    flushString(); lines << "PRESS INSERT"; break;
            case Qt::Key_Home:      flushString(); lines << "PRESS HOME"; break;
            case Qt::Key_End:       flushString(); lines << "PRESS END"; break;
            case Qt::Key_Escape:    flushString(); lines << "PRESS ESC"; break;
            case Qt::Key_Up:        flushString(); lines << "MOVE CURSOR UP 1"; break;
            case Qt::Key_Down:      flushString(); lines << "MOVE CURSOR DOWN 1"; break;
            case Qt::Key_Left:      flushString(); lines << "MOVE CURSOR LEFT 1"; break;
            case Qt::Key_Right:     flushString(); lines << "MOVE CURSOR RIGHT 1"; break;
            // Return/Enter keypress is recorded separately as an AID key — skip it here
            // to avoid emitting TYPE "\n" before PRESS ENTER
            case Qt::Key_Return:
            case Qt::Key_Enter:     flushString(); break;
            default:
                handled = false;
                break;
            }
            if (!handled) {
                if (!step.text.isEmpty()) {
                    pendingString += step.text;
                } else {
                    flushString();
                    lines << QString("# Unknown key: %1").arg(step.key);
                }
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
