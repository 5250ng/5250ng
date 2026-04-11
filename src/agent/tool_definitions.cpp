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

#include "tool_definitions.h"
#include "config.h"

namespace agent {

struct ToolDef {
    QString name;
    QString defaultDescription;
    QJsonObject (*schemaFn)();
};

static const ToolDef kAllTools[] = {
    {kToolConnect, kToolConnectDescription, toolConnectSchema},
    {kToolFindText, kToolFindTextDescription, toolFindTextSchema},
    {kToolGenerateScript, kToolGenerateScriptDescription, toolGenerateScriptSchema},
    {kToolGetCursorPosition, kToolGetCursorPositionDescription, toolGetCursorPositionSchema},
    {kToolGetFieldAt, kToolGetFieldAtDescription, toolGetFieldAtSchema},
    {kToolGetScreenSize, kToolGetScreenSizeDescription, toolGetScreenSizeSchema},
    {kToolListFiles, kToolListFilesDescription, toolListFilesSchema},
    {kToolLogin, kToolLoginDescription, toolLoginSchema},
    {kToolMoveCursor, kToolMoveCursorDescription, toolMoveCursorSchema},
    {kToolReadFile, kToolReadFileDescription, toolReadFileSchema},
    {kToolReadLine, kToolReadLineDescription, toolReadLineSchema},
    {kToolReadRegion, kToolReadRegionDescription, toolReadRegionSchema},
    {kToolReadScreen, kToolReadScreenDescription, toolReadScreenSchema},
    {kToolRunScript, kToolRunScriptDescription, toolRunScriptSchema},
    {kToolSendKeys, kToolSendKeysDescription, toolSendKeysSchema},
    {kToolSetCursorPosition, kToolSetCursorPositionDescription, toolSetCursorPositionSchema},
    {kToolTypeText, kToolTypeTextDescription, toolTypeTextSchema},
    {kToolWaitForText, kToolWaitForTextDescription, toolWaitForTextSchema},
    {kToolWriteFile, kToolWriteFileDescription, toolWriteFileSchema},
};

static QString effectiveDescription(const ToolDef &def, const AgentConfig &cfg) {
    ToolConfig tc = cfg.toolConfig(def.name);
    if (!tc.customDescription.isEmpty())
        return tc.customDescription;
    return def.defaultDescription;
}

QJsonArray anthropicToolsArray() {
    const auto &cfg = AgentConfig::instance();
    QJsonArray arr;
    for (const auto &def : kAllTools) {
        ToolConfig tc = cfg.toolConfig(def.name);
        if (!tc.enabled) continue;

        QJsonObject tool;
        tool["name"] = def.name;
        tool["description"] = effectiveDescription(def, cfg);
        tool["input_schema"] = def.schemaFn();
        arr.append(tool);
    }
    return arr;
}

QJsonArray openaiToolsArray() {
    const auto &cfg = AgentConfig::instance();
    QJsonArray arr;
    for (const auto &def : kAllTools) {
        ToolConfig tc = cfg.toolConfig(def.name);
        if (!tc.enabled) continue;

        QJsonObject func;
        func["name"] = def.name;
        func["description"] = effectiveDescription(def, cfg);
        func["parameters"] = def.schemaFn();

        QJsonObject tool;
        tool["type"] = "function";
        tool["function"] = func;
        arr.append(tool);
    }
    return arr;
}

} // namespace agent
