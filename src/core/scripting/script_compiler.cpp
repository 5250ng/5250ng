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

#include "script_compiler.h"

namespace core::scripting {

QString ScriptCompiler::macroToScript(const Macro &macro) {
    QStringList lines;

    lines << QString("# %1").arg(macro.name);
    if (!macro.description.isEmpty())
        lines << QString("# %1").arg(macro.description);
    lines << "";

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
        case MacroStep::KeyPress:
            if (!step.text.isEmpty()) {
                pendingString += step.text;
            } else {
                // Non-character key press — map Qt key to script keyword
                flushString();
                // Map special Qt keys to local key keywords
                switch (step.key) {
                case Qt::Key_Tab:       lines << "PRESS TAB"; break;
                case Qt::Key_Backtab:   lines << "PRESS BACKTAB"; break;
                case Qt::Key_Backspace: lines << "PRESS BACKSPACE"; break;
                case Qt::Key_Delete:    lines << "PRESS DELETE"; break;
                case Qt::Key_Insert:    lines << "PRESS INSERT"; break;
                case Qt::Key_Home:      lines << "PRESS HOME"; break;
                case Qt::Key_End:       lines << "PRESS END"; break;
                case Qt::Key_Escape:    lines << "PRESS ESC"; break;
                case Qt::Key_Up:        lines << "MOVE CURSOR UP 1"; break;
                case Qt::Key_Down:      lines << "MOVE CURSOR DOWN 1"; break;
                case Qt::Key_Left:      lines << "MOVE CURSOR LEFT 1"; break;
                case Qt::Key_Right:     lines << "MOVE CURSOR RIGHT 1"; break;
                default:
                    lines << QString("# Unknown key: %1").arg(step.key);
                    break;
                }
            }
            break;

        case MacroStep::AIDKey:
            flushString();
            lines << QString("PRESS %1").arg(aidByteToKeyword(step.aidByte));
            break;

        case MacroStep::Delay:
            flushString();
            if (step.delayMs > 0)
                lines << QString("WAIT %1").arg(step.delayMs);
            break;
        }
    }

    flushString();
    return lines.join("\n") + "\n";
}

QString ScriptCompiler::aidByteToKeyword(uint8_t aidByte) {
    switch (aidByte) {
    case 0xF1: return "ENTER";
    case 0x31: return "F1";
    case 0x32: return "F2";
    case 0x33: return "F3";
    case 0x34: return "F4";
    case 0x35: return "F5";
    case 0x36: return "F6";
    case 0x37: return "F7";
    case 0x38: return "F8";
    case 0x39: return "F9";
    case 0x3A: return "F10";
    case 0x3B: return "F11";
    case 0x3C: return "F12";
    case 0xB1: return "F13";
    case 0xB2: return "F14";
    case 0xB3: return "F15";
    case 0xB4: return "F16";
    case 0xB5: return "F17";
    case 0xB6: return "F18";
    case 0xB7: return "F19";
    case 0xB8: return "F20";
    case 0xB9: return "F21";
    case 0xBA: return "F22";
    case 0xBB: return "F23";
    case 0xBC: return "F24";
    case 0xF5: return "PAGEUP";
    case 0xF4: return "PAGEDOWN";
    case 0x70: return "ATTN";
    case 0x71: return "SYSREQ";
    case 0xF3: return "HELP";
    case 0xBD: return "CLEAR";
    case 0xF6: return "PRINT";
    default:   return QString("# Unknown AID: 0x%1").arg(aidByte, 2, 16, QChar('0'));
    }
}

} // namespace core::scripting
