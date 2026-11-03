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

#include "logger/logger.h"
#include "ui/dialogs/log_viewer.h"
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QtTest/QtTest>

// The application log viewer shows live lines like the loaded ones, keeps
// the user's place while they read further up, and holds a bounded history.
class TestLogViewer : public QObject {
    Q_OBJECT

  private slots:
    void liveLinesCarryTimestampAndLevel() {
        LogViewerDialog dialog;
        auto *text = dialog.findChild<QPlainTextEdit *>();
        QVERIFY(text);
        logger::Logger::instance()->info(QStringLiteral("live line 1"));
        const QString last = text->document()->lastBlock().text();
        QVERIFY2(QRegularExpression(R"(^\[\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3}\] \[INFO\] live line 1$)")
                     .match(last).hasMatch(),
                 qPrintable(last));
    }

    void historyIsBounded() {
        LogViewerDialog dialog;
        auto *text = dialog.findChild<QPlainTextEdit *>();
        QCOMPARE(text->maximumBlockCount(), logger::Logger::maxRecentLogs());
    }

    void scrolledUpViewKeepsItsPlace() {
        LogViewerDialog dialog;
        dialog.resize(600, 300);
        dialog.show();
        auto *text = dialog.findChild<QPlainTextEdit *>();
        for (int i = 0; i < 300; ++i)
            logger::Logger::instance()->info(QStringLiteral("filler %1").arg(i));
        QTest::qWait(20);
        QScrollBar *bar = text->verticalScrollBar();
        QVERIFY(bar->maximum() > 0);
        QCOMPARE(bar->value(), bar->maximum()); // followed the tail

        bar->setValue(0); // the user scrolls up to read
        for (int i = 0; i < 5; ++i)
            logger::Logger::instance()->info(QStringLiteral("while reading %1").arg(i));
        QTest::qWait(20);
        QCOMPARE(bar->value(), 0);

        bar->setValue(bar->maximum()); // back at the bottom: follow again
        for (int i = 0; i < 5; ++i)
            logger::Logger::instance()->info(QStringLiteral("at the bottom %1").arg(i));
        QTest::qWait(20);
        QCOMPARE(bar->value(), bar->maximum());
    }
};

QTEST_MAIN(TestLogViewer)
#include "test_log_viewer.moc"
