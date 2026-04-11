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

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace agent {

struct ToolCall {
    QString id;         // "toolu_xxx" (Anthropic) or "call_xxx" (OpenAI)
    QString name;       // "run_5250script"
    QString arguments;  // raw JSON string of arguments
    QString textBefore; // any text the LLM emitted alongside the tool call

    /// Parse the arguments JSON and return the "script" field.
    QString scriptText() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("script").toString();
    }

    /// Parse the arguments JSON and return the "task" field.
    QString taskText() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("task").toString();
    }

    /// Parse the arguments JSON and return the "path" field.
    QString filePath() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("path").toString();
    }

    /// Parse the arguments JSON and return the "content" field.
    QString fileContent() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("content").toString();
    }

    /// Parse the arguments JSON and return the "text" field.
    QString typeText() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("text").toString();
    }

    /// Parse the arguments JSON and return the "keys" field.
    QString keysText() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("keys").toString();
    }

    /// Parse the arguments JSON and return the "row" field.
    int row() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("row").toInt();
    }

    /// Parse the arguments JSON and return the "col" field.
    int col() const {
        QJsonObject obj = QJsonDocument::fromJson(arguments.toUtf8()).object();
        return obj.value("col").toInt();
    }
};

struct ToolResult {
    QString toolCallId;
    bool success;
    QString output; // log messages, error text, etc.
};

} // namespace agent
