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
#include <QVector>

namespace core::scripting {

enum class TokenType {
    // Input — text
    TYPE, PRESS,

    // AID keys (lock keyboard, wait for host)
    ENTER, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    PAGEUP, PAGEDOWN, ATTN, SYSREQ, HELP, CLEAR, PRINT,

    // Local keys (no host round-trip)
    TAB, BACKTAB, BACKSPACE, DELETE_KEY, INSERT, HOME, END, ESC,
    FIELDPLUS, FIELDMINUS, FIELDEXIT, DUP, ERASEINPUT, ERASEFIELD, ERASEEOF,

    // Cursor movement
    MOVE,

    // Punctuation (for parenthesized syntax)
    LPAREN, COMMA, RPAREN,

    // Screen inspection
    EXPECT, EXTRACT,

    // Timing
    WAIT,

    // Global settings
    GLOBAL,

    // Variables
    SET, INC, DEC, ADD,

    // Control flow
    IF, ELSE, ENDIF, WHILE, ENDWHILE, REPEAT, ENDREPEAT,
    LABEL, GOTO,

    // Functions
    DEF, ENDDEF, CALL, RETURN,

    // Error handling
    ON, ABORT,

    // Utility
    LOG, PAUSE, INPUT,

    // Sub-tokens (used within compound statements)
    TEXT, AT, ROW, CURSOR, KEYBOARD, UNLOCKED, ERRORLOCKED,
    FIELD, CONTAINS, MESSAGEWAITING, NOT, LENGTH, FROM,
    ERROR, TIMEOUT, INPUTFIELD, NEXT, PREVIOUS, COL, LINE,
    UP, DOWN, LEFT, RIGHT,
    DELAY, JITTER, EXPECT_TIMEOUT, ISSET,

    // Comparison operators
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,

    // Character key constant (KEY_A, KEY_0, KEY_SPACE, etc.)
    KEY_CHAR,

    // Literals
    STRING_LITERAL,  // "quoted string"
    NUMBER_LITERAL,  // 123
    VARIABLE,        // $FOO

    // End of line / end of file
    EOL, EOF_TOKEN,

    // Unknown character (unexpected input — lets the parser emit an error)
    UNKNOWN
};

struct ScriptToken {
    TokenType type = TokenType::EOF_TOKEN;
    QString value;       // Raw text of the token
    int line = 0;        // 1-based line number
    int column = 0;      // 1-based column

    ScriptToken() = default;
    ScriptToken(TokenType t, const QString &v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
};

using TokenLine = QVector<ScriptToken>;

} // namespace core::scripting
