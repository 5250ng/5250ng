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

#include "ui/dialogs/script_editor_window.h"
#include "ui/widgets/QScriptEditorWidget/QScriptEditorWidget.h"
#include <QApplication>
#include <QDialog>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>
#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/resource.h>
#endif

// A script save that fails part-way (full disk, size limit) must report
// the failure and keep the previous file; QSaveFile::commit() alone does not
// notice a failed final flush on Qt 6.4.
class TestScriptSaveFlush : public QObject {
    Q_OBJECT

  private slots:
    void failedSaveKeepsPreviousScript() {
#ifndef Q_OS_UNIX
        QSKIP("Needs RLIMIT_FSIZE to make the write fail");
#else
        QTemporaryDir dir;
        const QString path = dir.filePath("s.5250script");
        const QByteArray original = QByteArray("WAIT 1\n").repeated(20);
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(original);
        }
        ScriptEditorWindow window;
        QVERIFY(window.openFile(path));
        ui::widgets::QScriptEditorWidget *editor = window.currentEditor();
        editor->editor()->moveCursor(QTextCursor::End);
        editor->editor()->insertPlainText(QString("PRESS ENTER\n").repeated(50));

        // Dismiss the "Cannot write" warning the failed save shows.
        QTimer dismiss;
        connect(&dismiss, &QTimer::timeout, this, []() {
            if (auto *w = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                w->reject();
        });
        dismiss.start(50);

        struct rlimit saved;
        QCOMPARE(getrlimit(RLIMIT_FSIZE, &saved), 0);
        auto oldHandler = std::signal(SIGXFSZ, SIG_IGN);
        struct rlimit small = saved;
        small.rlim_cur = 64;
        QCOMPARE(setrlimit(RLIMIT_FSIZE, &small), 0);
        const bool ok = window.saveTab(0);
        setrlimit(RLIMIT_FSIZE, &saved);
        std::signal(SIGXFSZ, oldHandler);

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY2(!ok, "saveTab reported success for a failed write");
        QCOMPARE(f.readAll(), original);
#endif
    }
};

QTEST_MAIN(TestScriptSaveFlush)
#include "test_script_save_flush.moc"
