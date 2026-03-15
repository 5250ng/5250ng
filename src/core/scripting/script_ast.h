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

#include <QMap>
#include <QString>
#include <QVector>
#include <memory>

namespace core::scripting {

enum class NodeType {
    Script,         // Root node

    // Input
    TypeInput,      // TYPE "text"
    CharInput,      // PRESS char (single character)

    // Keys
    AIDKey,         // ENTER, F1-F24, PAGEUP, PAGEDOWN, ATTN, SYSREQ, HELP, CLEAR, PRINT
    LocalKey,       // TAB, BACKTAB, BACKSPACE, DELETE, INSERT, HOME, END,
                    // FIELDPLUS, FIELDMINUS, FIELDEXIT, DUP, ERASEINPUT, ERASEFIELD, ERASEEOF

    // Cursor
    MoveCursorAt,   // MOVE CURSOR AT/UP/DOWN/LEFT/RIGHT ...

    // Screen inspection
    Expect,         // EXPECT ...  (many variants)
    Extract,        // EXTRACT ...

    // Timing
    Wait,           // WAIT ms

    // Global settings
    GlobalDelay,         // GLOBAL DELAY ms
    GlobalJitter,        // GLOBAL JITTER min max
    GlobalExpectTimeout, // GLOBAL EXPECT_TIMEOUT ms

    // Variables
    Set,            // SET $var value
    Inc,            // INC $var
    Dec,            // DEC $var
    Add,            // ADD $var value

    // Control flow
    If,             // IF condition ... ELSE ... ENDIF
    While,          // WHILE condition ... ENDWHILE
    Repeat,         // REPEAT n ... ENDREPEAT
    Label,          // LABEL name
    Goto,           // GOTO name

    // Error handling
    OnTimeout,      // ON TIMEOUT GOTO label
    OnError,        // ON ERROR GOTO label
    Abort,          // ABORT ["message"]

    // Utility
    Log,            // LOG "message"
    Pause           // PAUSE
};

// EXPECT sub-types
enum class ExpectType {
    TextAnywhere,       // EXPECT TEXT "str"
    TextAtPos,          // EXPECT TEXT "str" AT row col
    TextAtRow,          // EXPECT TEXT "str" AT ROW row
    CursorAtPos,        // EXPECT CURSOR AT row col
    CursorAtRow,        // EXPECT CURSOR AT ROW row
    KeyboardUnlocked,   // EXPECT KEYBOARD UNLOCKED
    KeyboardErrorLocked,// EXPECT KEYBOARD ERRORLOCKED
    FieldContains,      // EXPECT FIELD AT row col CONTAINS "str"
    MessageWaiting,     // EXPECT MESSAGEWAITING
    // Negated variants set the `negated` flag
};

// EXTRACT sub-types
enum class ExtractType {
    FromPosition,   // EXTRACT $var FROM row col LENGTH n
    FieldAt,        // EXTRACT $var FIELD AT row col
    CursorRow,      // EXTRACT $var CURSOR ROW
    CursorCol,      // EXTRACT $var CURSOR COL (implicit from ROW pattern)
};

// MOVE CURSOR AT sub-modes
enum class MoveCursorMode {
    Position,       // MOVE CURSOR AT (row,col)
    FieldIndex,     // MOVE CURSOR AT INPUTFIELD n
    NextField,      // MOVE CURSOR AT NEXT INPUTFIELD
    PreviousField,  // MOVE CURSOR AT PREVIOUS INPUTFIELD
    StepUp,         // MOVE CURSOR UP [n]
    StepDown,       // MOVE CURSOR DOWN [n]
    StepLeft,       // MOVE CURSOR LEFT [n]
    StepRight,      // MOVE CURSOR RIGHT [n]
};

// Comparison operator for IF/WHILE conditions
enum class CompareOp {
    Eq, Ne, Lt, Gt, Le, Ge
};

struct ASTNode {
    NodeType type;
    int line = 0;  // Source line number for error reporting

    // Generic string payload (key name, label name, log message, etc.)
    QString stringValue;

    // Numeric payload (row, col, delay, count, etc.)
    int intValue = 0;
    int intValue2 = 0;  // Second numeric (e.g., col in MOVETO row col)

    // Variable name (for SET, INC, DEC, ADD, EXTRACT, IF, WHILE)
    QString varName;

    // EXPECT specifics
    ExpectType expectType = ExpectType::TextAnywhere;
    bool negated = false;

    // MOVE CURSOR AT specifics
    MoveCursorMode moveCursorMode = MoveCursorMode::Position;

    // EXTRACT specifics
    ExtractType extractType = ExtractType::FromPosition;

    // Condition (for IF, WHILE)
    QString condLeft;       // Left operand (variable name or literal)
    CompareOp condOp = CompareOp::Eq;
    QString condRight;      // Right operand (variable name or literal)

    // AID key byte (for AIDKey nodes)
    uint8_t aidByte = 0;

    // Child nodes (for block statements: IF body, ELSE body, WHILE body, REPEAT body, Script root)
    QVector<std::shared_ptr<ASTNode>> children;

    // ELSE branch for IF nodes
    QVector<std::shared_ptr<ASTNode>> elseChildren;
};

struct ParseError {
    int line;
    QString message;
};

struct ParseResult {
    std::shared_ptr<ASTNode> root;
    QVector<ParseError> errors;
    QMap<QString, int> labels;  // label name -> index in root->children

    bool hasErrors() const { return !errors.isEmpty(); }
};

} // namespace core::scripting
