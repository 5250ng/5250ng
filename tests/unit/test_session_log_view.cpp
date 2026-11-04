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

#include "core/session_logger.h"
#include "ui/dialogs/log_viewer.h"
#include "ui/session_log_view.h"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

// Regression tests for issue #352: Session > View Session Logs opened the
// application log, so a tab's own session log could not be viewed.
class TestSessionLogView : public QObject {
    Q_OBJECT

    static QString viewerText(LogViewerDialog *viewer) {
        auto *text = viewer->findChild<QPlainTextEdit *>();
        return text ? text->toPlainText() : QString();
    }

  private slots:
    void showsTheTabsLogAndFollowsIt() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("session.log");
        core::SessionLogger logger;
        QVERIFY(logger.start(path));
        logger.logEvent("first event");

        QWidget parent;
        LogViewerDialog *viewer = ui::showSessionLog(&parent, &logger, "PUB400");
        QVERIFY(viewer);
        QCOMPARE(viewer->filePath(), path);
        QVERIFY(viewer->windowTitle().contains("PUB400"));
        QVERIFY2(viewerText(viewer).contains("[EVENT] first event"), qPrintable(viewerText(viewer)));
        // Not the application log's diagnostic save
        for (QPushButton *b : viewer->findChildren<QPushButton *>())
            if (b->text().contains("Diagnostic"))
                QVERIFY(!b->isVisible());

        // Lines written while the window is open appear in it
        logger.logEvent("second event");
        QTRY_VERIFY_WITH_TIMEOUT(viewerText(viewer).contains("[EVENT] second event"), 3000);
        QCOMPARE(viewerText(viewer).count("first event"), 1);
        delete viewer;
    }

    void stoppedLogIsStillShown() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("session.log");
        core::SessionLogger logger;
        QVERIFY(logger.start(path));
        logger.logEvent("before stop");
        logger.stop();
        QWidget parent;
        LogViewerDialog *viewer = ui::showSessionLog(&parent, &logger, "PUB400");
        QVERIFY(viewer);
        QVERIFY(viewer->windowTitle().contains("stopped"));
        QVERIFY(viewerText(viewer).contains("Session log ended"));
        delete viewer;
    }

    // Without a log file, say how to start one instead of opening a window
    void noLogExplainsHowToStart() {
        core::SessionLogger logger;
        QWidget parent;
        QString shown;
        QTimer::singleShot(0, this, [&shown] {
            // Poll for the message box, then dismiss it
            auto *poll = new QTimer;
            QObject::connect(poll, &QTimer::timeout, poll, [poll, &shown] {
                if (QWidget *modal = QApplication::activeModalWidget()) {
                    for (QLabel *l : modal->findChildren<QLabel *>())
                        shown += l->text();
                    modal->close();
                    poll->deleteLater();
                }
            });
            poll->start(20);
            QTimer::singleShot(3000, poll, [poll] { poll->deleteLater(); });
        });
        QVERIFY(!ui::showSessionLog(&parent, &logger, "PUB400"));
        QVERIFY2(shown.contains("Session Logging"), qPrintable(shown));
    }

    // Session logs are written in text mode, so lines end with CRLF on
    // Windows; neither the first read nor a later one adds blank lines
    void crlfLinesHaveNoBlankLines() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("crlf.log");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("a\r\nb\r\n");
        file.flush();
        LogViewerDialog viewer(path, "Session Log - crlf");
        QCOMPARE(viewerText(&viewer), QStringLiteral("a\nb"));
        file.write("c\r");
        file.flush();
        QTest::qWait(2 * LogViewerDialog::kFollowIntervalMs);
        file.write("\nd\r\n");
        file.close();
        QTRY_COMPARE_WITH_TIMEOUT(viewerText(&viewer), QStringLiteral("a\nb\nc\nd"), 3000);
    }

    // When the 2 MiB tail starts exactly at a line, that line is kept
    void tailStartingAtALineKeepsIt() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("edge.log");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("dropped\n");
        // 2 MiB of 128-byte lines (fewer than kMaxFileLines) follow it
        const QByteArray line = QByteArray(127, 'y') + '\n';
        const qint64 lines = LogViewerDialog::kMaxInitialBytes / 128;
        QVERIFY(lines < LogViewerDialog::kMaxFileLines);
        for (qint64 i = 0; i < lines; ++i)
            file.write(line);
        file.close();
        LogViewerDialog viewer(path, "Session Log - edge");
        const QStringList shown = viewerText(&viewer).split('\n');
        QCOMPARE(shown.size(), int(lines));
        QVERIFY(!viewerText(&viewer).contains("dropped"));
    }

    // A large log opens on its last part, starting at a line boundary
    void largeLogOpensOnItsTail() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("big.log");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray line(99, 'x');
        const qint64 lines = LogViewerDialog::kMaxInitialBytes / 100 + 500;
        for (qint64 i = 0; i < lines; ++i)
            file.write(line + '\n');
        file.write("last line\n");
        file.close();
        LogViewerDialog viewer(path, "Session Log - big");
        const QString text = viewerText(&viewer);
        QVERIFY(text.endsWith("last line"));
        const QStringList shown = text.split('\n');
        for (const QString &l : shown.mid(0, shown.size() - 1))
            QCOMPARE(l.size(), 99);
        QVERIFY(shown.size() <= LogViewerDialog::kMaxFileLines);
    }
};

QTEST_MAIN(TestSessionLogView)
#include "test_session_log_view.moc"
