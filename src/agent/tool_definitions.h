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

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace agent {

inline const QString kToolRunScript = QStringLiteral("run_5250script");

inline const QString kToolRunScriptDescription = QStringLiteral(
    "Execute a 5250script on the current terminal session. "
    "The script language supports: "
    "TYPE \"text\", PRESS ENTER/F1-F24/PAGEUP/PAGEDOWN/TAB/BACKSPACE, "
    "MOVE CURSOR AT (row,col), GOTO INPUTFIELD n/NEXT/PREVIOUS, "
    "EXPECT TEXT \"text\"/EXPECT TEXT \"text\" AT row col/EXPECT KEYBOARD UNLOCKED/EXPECT SCREEN CONTAINS \"text\", "
    "WAIT milliseconds, LOG \"message\", "
    "SET $var \"value\", IF $var == \"value\" ... ENDIF, WHILE ... ENDWHILE, REPEAT n TIMES ... ENDREPEAT, "
    "GLOBAL EXPECT_TIMEOUT milliseconds, GLOBAL DELAY milliseconds. "
    "The script interacts with the live AS/400 session. "
    "Always use EXPECT KEYBOARD UNLOCKED before typing or pressing keys after a screen transition. "
    "Row and column numbers are 1-based.");

/// Build the input_schema / parameters object for the run_5250script tool.
inline QJsonObject toolRunScriptSchema() {
    QJsonObject scriptProp;
    scriptProp["type"] = "string";
    scriptProp["description"] = "The 5250script source code to execute";

    QJsonObject properties;
    properties["script"] = scriptProp;

    QJsonObject schema;
    schema["type"] = "object";
    schema["properties"] = properties;
    schema["required"] = QJsonArray{"script"};
    return schema;
}

/// Build the Anthropic-format tools array.
inline QJsonArray anthropicToolsArray() {
    QJsonObject tool;
    tool["name"] = kToolRunScript;
    tool["description"] = kToolRunScriptDescription;
    tool["input_schema"] = toolRunScriptSchema();
    return QJsonArray{tool};
}

/// Build the OpenAI-format tools array.
inline QJsonArray openaiToolsArray() {
    QJsonObject function;
    function["name"] = kToolRunScript;
    function["description"] = kToolRunScriptDescription;
    function["parameters"] = toolRunScriptSchema();

    QJsonObject tool;
    tool["type"] = "function";
    tool["function"] = function;
    return QJsonArray{tool};
}

} // namespace agent
