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

#include "agent/tool_definitions.h"
#include "core/ebcdic.h"
#include "mcp/McpSessionRegistry.h"
#include "mcp/McpToolHandler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer_adapter.h"
#include <QJsonArray>
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;

// Issue #481: MCP read tools decoded every session with the process-wide
// code page, which is the active tab's. A background session on another
// code page must be read with its own.
class TestMcpSessionCodePage : public QObject {
    Q_OBJECT

    static QString text(const QJsonObject &result) {
        return result.value("content").toArray().at(0).toObject().value("text").toString();
    }

  private slots:
    void readsUseTheSessionsCodePage() {
        // The active tab is on CP037; this session is on CP273, where bytes
        // 4A 5A C0 are A, U and a with umlaut (CP037: cent, !, left brace;
        // Python's cp037/cp273 codecs agree)
        core::EBCDIC::setCodePage(core::CodePage::ID::CP037);
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setCodePage(core::CodePage::ID::CP273);
        auto *buf = widget.screenBuffer();
        buf->writeChar(3, 10, 0x4A);
        buf->writeChar(3, 11, 0x5A);
        buf->writeChar(3, 12, 0xC0);
        const QString expected = QString(QChar(0x00C4)) + QChar(0x00DC) + QChar(0x00E4);

        mcp::McpSessionRegistry registry;
        mcp::McpToolHandler handler(&registry);
        mcp::McpSessionInfo info;
        info.sessionId = QStringLiteral("cp273-session");
        registry.addSession(info);
        handler.onSessionCreated(info.sessionId, &widget);
        const QJsonObject session{{"session_id", info.sessionId}};

        const QString screen = text(handler.callTool(agent::kToolReadScreen, session));
        QVERIFY2(screen.contains(expected), qPrintable(screen));

        QJsonObject line = session;
        line["row"] = 3;
        const QString row = text(handler.callTool(agent::kToolReadLine, line));
        QVERIFY2(row.contains(expected), qPrintable(row));

        QJsonObject region = session;
        region["row"] = 3;
        region["col"] = 10;
        region["numRows"] = 1;
        region["numCols"] = 3;
        const QString area = text(handler.callTool(agent::kToolReadRegion, region));
        QVERIFY2(area.contains(expected), qPrintable(area));

        buf->setField(3, 10, 3, false);
        QJsonObject at = session;
        at["row"] = 3;
        at["col"] = 10;
        const QString field = text(handler.callTool(agent::kToolGetFieldAt, at));
        QVERIFY2(field.contains(expected), qPrintable(field));

        // Scripts (wait_for_text, login, run_script) read through the adapter
        core::scripting::ScreenBufferAdapter adapter(&widget);
        QCOMPARE(adapter.readText(3, 10, 3), expected);
        QCOMPARE(adapter.readFieldText(3, 10), expected);

        QJsonObject find = session;
        find["text"] = expected;
        const QString found = text(handler.callTool(agent::kToolFindText, find));
        QVERIFY2(found.contains("3") && !found.contains("not found", Qt::CaseInsensitive),
                 qPrintable(found));
    }
};

QTEST_MAIN(TestMcpSessionCodePage)
#include "test_mcp_session_code_page.moc"
