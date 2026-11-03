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
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;

// The screen history keeps one snapshot per screen the host draws, not one
// per character written while drawing it.
class TestScreenHistorySnapshots : public QObject {
    Q_OBJECT

  private slots:
    void oneSnapshotPerScreen() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        auto *history = widget.screenHistory();
        history->clear();

        drawScreen(widget, QStringLiteral("FIRST SCREEN"));
        QTRY_COMPARE_WITH_TIMEOUT(history->count(), 1, 2000);
        drawScreen(widget, QStringLiteral("SECOND SCREEN"));
        QTRY_COMPARE_WITH_TIMEOUT(history->count(), 2, 2000);
        QTest::qWait(300);
        QCOMPARE(history->count(), 2);
        QVERIFY(QString::fromUtf8(history->at(0).cellData).contains("SECOND SCREEN"));
        QVERIFY(QString::fromUtf8(history->at(1).cellData).contains("FIRST SCREEN"));
    }

    // Redrawing the same screen adds nothing
    void identicalScreenIsNotRecordedTwice() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.screenHistory()->clear();
        drawScreen(widget, QStringLiteral("SAME"));
        QTRY_COMPARE_WITH_TIMEOUT(widget.screenHistory()->count(), 1, 2000);
        drawScreen(widget, QStringLiteral("SAME"));
        QTest::qWait(300);
        QCOMPARE(widget.screenHistory()->count(), 1);
    }

  private:
    // Writes the text cell by cell, as the host data stream does
    static void drawScreen(Q5250ScreenWidget &widget, const QString &text) {
        auto *buf = widget.screenBuffer();
        buf->clear();
        for (int i = 0; i < text.size(); ++i)
            buf->writeChar(0, i, core::EBCDIC::charToEBCDIC(text[i]));
    }
};

QTEST_MAIN(TestScreenHistorySnapshots)
#include "test_screen_history_snapshots.moc"
