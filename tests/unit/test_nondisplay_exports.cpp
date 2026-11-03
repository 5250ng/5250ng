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

#include "core/ebcdic.h"
#include "mcp/McpSessionRegistry.h"
#include "mcp/McpToolHandler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer_adapter.h"
#include <QJsonArray>
#include <QStandardPaths>
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #381 (part of #242): MCP screen tools and
// 5250Script screen reads returned the characters of non-display cells,
// such as a typed password, which the screen itself does not show.
class TestNonDisplayExports : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init();
    void cleanup();

    void testMcpReadersBlankNonDisplayCells_data();
    void testMcpReadersBlankNonDisplayCells();
    void testFindTextSkipsNonDisplayCells();
    void testScriptReadsBlankNonDisplayCells();
    void testHostStillGetsFieldData();

  private:
    QString call(const QString &tool, QJsonObject args);

    Q5250ScreenWidget *m_widget = nullptr;
    mcp::McpSessionRegistry *m_registry = nullptr;
    mcp::McpToolHandler *m_handler = nullptr;
};

static const QString kSessionId = QStringLiteral("test-session");

void TestNonDisplayExports::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_registry = new mcp::McpSessionRegistry();
    m_handler = new mcp::McpToolHandler(m_registry);
    mcp::McpSessionInfo info;
    info.sessionId = kSessionId;
    m_registry->addSession(info);
    m_registry->setDisplayWidget(kSessionId, m_widget);

    // "Password" label, then a non-display input field at row 5 col 20
    // holding a typed password
    auto *buf = m_widget->screenBuffer();
    const QString label = QStringLiteral("Password");
    for (int i = 0; i < label.size(); ++i)
        buf->writeChar(4, 5 + i, core::EBCDIC::charToEBCDIC(label[i]));
    buf->setField(4, 20, 10, false);
    ui::widgets::CellAttributes hidden;
    hidden.nonDisplay = true;
    const QString secret = QStringLiteral("SECRET");
    for (int i = 0; i < 10; ++i)
        buf->writeChar(4, 20 + i, i < secret.size() ? core::EBCDIC::charToEBCDIC(secret[i]) : 0x00,
                       hidden);
}

void TestNonDisplayExports::cleanup() {
    delete m_handler;
    delete m_registry;
    delete m_widget;
}

QString TestNonDisplayExports::call(const QString &tool, QJsonObject args) {
    args["session_id"] = kSessionId;
    const QJsonObject result = m_handler->callTool(tool, args);
    return result.value("content").toArray().at(0).toObject().value("text").toString();
}

void TestNonDisplayExports::testMcpReadersBlankNonDisplayCells_data() {
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QJsonObject>("args");
    QTest::newRow("read_screen") << "read_screen" << QJsonObject{};
    QTest::newRow("read_line") << "read_line" << QJsonObject{{"row", 4}};
    QTest::newRow("read_region")
        << "read_region" << QJsonObject{{"row", 4}, {"col", 0}, {"numRows", 1}, {"numCols", 40}};
    QTest::newRow("get_field_at") << "get_field_at" << QJsonObject{{"row", 4}, {"col", 20}};
}

void TestNonDisplayExports::testMcpReadersBlankNonDisplayCells() {
    QFETCH(QString, tool);
    QFETCH(QJsonObject, args);
    const QString text = call(tool, args);
    QVERIFY2(!text.contains(QStringLiteral("SECRET")), qPrintable(text));
    if (tool != QStringLiteral("get_field_at"))
        QVERIFY2(text.contains(QStringLiteral("Password")), qPrintable(text)); // visible text kept
}

void TestNonDisplayExports::testFindTextSkipsNonDisplayCells() {
    QVERIFY(!call(QStringLiteral("find_text"), {{"text", "SECRET"}}).contains(QStringLiteral("row")));
}

void TestNonDisplayExports::testScriptReadsBlankNonDisplayCells() {
    // Scripts run from the Scripts menu and through run_5250script read here
    core::scripting::ScreenBufferAdapter adapter(m_widget);
    QVERIFY(!adapter.readText(4, 20, 10).contains(QStringLiteral("SECRET")));
    QVERIFY(!adapter.readFieldText(4, 20).contains(QStringLiteral("SECRET")));
    QVERIFY(adapter.readText(4, 5, 8) == QStringLiteral("Password"));
}

void TestNonDisplayExports::testHostStillGetsFieldData() {
    // The field data sent to the host is the raw value: the password itself
    auto *buf = m_widget->screenBuffer();
    const QByteArray raw = buf->getFieldData(buf->getField(4, 20));
    QVERIFY(core::EBCDIC::ebcdicToString(raw).startsWith(QStringLiteral("SECRET")));
}

QTEST_MAIN(TestNonDisplayExports)
#include "test_nondisplay_exports.moc"
