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

#include <QString>

namespace mcp {

// Escape a string for embedding inside a 5250script double-quoted literal.
// The lexer (ScriptLexer::readStringLiteral) recognizes the escapes \" \n \r
// \t and \\, so a backslash MUST be escaped first — otherwise a trailing
// backslash in the input escapes the closing quote (unterminated string) and
// a raw newline injects new physical script lines (statement injection).
// Escaping only the double quote handles neither case.
inline QString escapeScriptString(const QString &in) {
    QString out;
    out.reserve(in.size());
    for (QChar c : in) {
        switch (c.unicode()) {
        case '\\': out += QStringLiteral("\\\\"); break;
        case '"':  out += QStringLiteral("\\\""); break;
        case '\n': out += QStringLiteral("\\n"); break;
        case '\r': out += QStringLiteral("\\r"); break;
        case '\t': out += QStringLiteral("\\t"); break;
        default:   out += c; break;
        }
    }
    return out;
}

} // namespace mcp
