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

// Regression tests for issue #406: read_region checked row + numRows and
// col + numCols, which overflow for huge arguments and can let the check
// pass.
class TestReadRegionBounds : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void hugeArgumentsAreRefused_data() {
        QTest::addColumn<int>("row");
        QTest::addColumn<int>("col");
        QTest::addColumn<int>("numRows");
        QTest::addColumn<int>("numCols");
        QTest::newRow("row INT_MAX") << INT_MAX << 0 << 1 << 1;
        QTest::newRow("numRows INT_MAX") << 1 << 0 << INT_MAX << 1;
        QTest::newRow("col INT_MAX") << 0 << INT_MAX << 1 << 1;
        QTest::newRow("numCols INT_MAX") << 0 << 1 << 1 << INT_MAX;
        QTest::newRow("just past the edge") << 20 << 0 << 5 << 10;
    }

    void hugeArgumentsAreRefused() {
        QFETCH(int, row);
        QFETCH(int, col);
        QFETCH(int, numRows);
        QFETCH(int, numCols);
        QVERIFY(call(row, col, numRows, numCols).value("isError").toBool());
    }

    void validRegionIsRead() {
        QVERIFY(!call(0, 0, 24, 80).value("isError").toBool());
        QVERIFY(!call(23, 79, 1, 1).value("isError").toBool());
    }

  private:
    QJsonObject call(int row, int col, int numRows, int numCols) {
        ui::widgets::Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        mcp::McpSessionRegistry registry;
        mcp::McpToolHandler handler(&registry);
        mcp::McpSessionInfo info;
        info.sessionId = QStringLiteral("s");
        registry.addSession(info);
        registry.setDisplayWidget(QStringLiteral("s"), &widget);
        return handler.callTool(QStringLiteral("read_region"),
                                {{"session_id", "s"}, {"row", row}, {"col", col},
                                 {"numRows", numRows}, {"numCols", numCols}});
    }
};

QTEST_MAIN(TestReadRegionBounds)
#include "test_read_region_bounds.moc"
