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

#include "mcp/McpSessionRegistry.h"
#include "mcp/McpToolHandler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QJsonArray>
#include <QStandardPaths>
#include <QtTest/QtTest>
#include <climits>

using ui::widgets::KeyboardState;

// Regression tests for issue #402: move_cursor built one script line per
// row or column requested, unbounded, so a huge count exhausted memory.
class TestMoveCursorBounds : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init();
    void cleanup();

    void outOfRangeIsRefused_data();
    void outOfRangeIsRefused();
    void inRangeStillMoves();
    void columnsWrapAcrossRows();

  private:
    QJsonObject call(int rows, int cols);

    ui::widgets::Q5250ScreenWidget *m_widget = nullptr;
    mcp::McpSessionRegistry *m_registry = nullptr;
    mcp::McpToolHandler *m_handler = nullptr;
};

void TestMoveCursorBounds::init() {
    m_widget = new ui::widgets::Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_widget->setKeyboardState(KeyboardState::Unlocked);
    m_widget->moveCursor(10, 10);
    m_registry = new mcp::McpSessionRegistry();
    m_handler = new mcp::McpToolHandler(m_registry);
    mcp::McpSessionInfo info;
    info.sessionId = QStringLiteral("s");
    m_registry->addSession(info);
    m_registry->setDisplayWidget(QStringLiteral("s"), m_widget);
}

void TestMoveCursorBounds::cleanup() {
    delete m_handler;
    delete m_registry;
    delete m_widget;
}

QJsonObject TestMoveCursorBounds::call(int rows, int cols) {
    return m_handler->callTool(QStringLiteral("move_cursor"),
                               {{"session_id", "s"}, {"rows", rows}, {"cols", cols}});
}

void TestMoveCursorBounds::outOfRangeIsRefused_data() {
    QTest::addColumn<int>("rows");
    QTest::addColumn<int>("cols");
    QTest::newRow("200 rows") << 200 << 0;
    QTest::newRow("cols beyond the screen") << 0 << 24 * 80;
    QTest::newRow("INT_MAX rows") << INT_MAX << 0;
    QTest::newRow("INT_MIN rows") << INT_MIN << 0;
    QTest::newRow("INT_MAX cols") << 0 << INT_MAX;
    QTest::newRow("INT_MIN cols") << 0 << INT_MIN;
}

void TestMoveCursorBounds::outOfRangeIsRefused() {
    QFETCH(int, rows);
    QFETCH(int, cols);
    QElapsedTimer t;
    t.start();
    const QJsonObject r = call(rows, cols);
    QVERIFY(r.value("isError").toBool());
    QVERIFY(t.elapsed() < 2000);
    QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(10, 10));
}

void TestMoveCursorBounds::inRangeStillMoves() {
    const QJsonObject r = call(2, -3);
    QVERIFY2(!r.value("isError").toBool(),
             qPrintable(r.value("content").toArray().at(0).toObject().value("text").toString()));
    QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(7, 12));
}

void TestMoveCursorBounds::columnsWrapAcrossRows() {
    // 100 columns right from (10,10) on 80 columns ends on the next row
    const QJsonObject r = call(0, 100);
    QVERIFY(!r.value("isError").toBool());
    QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(30, 11));
}

QTEST_MAIN(TestMoveCursorBounds)
#include "test_move_cursor_bounds.moc"
