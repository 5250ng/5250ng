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

#include "session/config.h"
#include "ui/dialogs/session_settings_dialog.h"
#include "ui/dialogs/settings_dialog.h"
#include "ui/main_window.h"
#include "ui/themes/manager.h"
#include "ui/themes/terminal_theme_manager.h"

#include <QApplication>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

using ui::themes::TerminalThemeManager;

// Regression test for issue #390 through the real Settings dialog: Apply of
// an untouched built-in must not create a theme, and repeated Apply of an
// edited built-in must update one derived theme, not add a new one each time.
class TestThemeEditorApply : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        ui::themes::ThemeManager::instance().loadBuiltinThemes();
        TerminalThemeManager::instance().loadBuiltinThemes();
    }

    void applyFromThemePage() {
        QTcpServer host;
        QVERIFY(host.listen(QHostAddress::LocalHost, 0));
        int connections = 0;
        connect(&host, &QTcpServer::newConnection, this, [&]() {
            while (QTcpSocket *socket = host.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });
        session::SessionConfig config;
        config.setName(QStringLiteral("theme-editor"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(host.serverPort());
        config.setTerminalThemeId(QStringLiteral("classic_green"));

        MainWindow window;
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);

        auto derivedThemes = []() {
            QStringList ids;
            auto &mgr = TerminalThemeManager::instance();
            for (const QString &id : mgr.availableThemes())
                if (mgr.theme(id).parentThemeId == QStringLiteral("classic_green"))
                    ids << id;
            return ids;
        };
        const int before = derivedThemes().size();

        QStringList afterUntouched, afterFirstEdit, afterSecondEdit;
        QString shownAfterEdit;
        // Drive the page once the modal dialog is up (retrying until it is)
        QTimer driver;
        connect(&driver, &QTimer::timeout, this, [&]() {
            auto *dlg = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
            if (!dlg) return;
            driver.stop();
            SessionSettingsDialog *editor = dlg->terminalThemeEditor();
            editor->onApplyToAll(); // what Save runs; the session's built-in, untouched
            afterUntouched = derivedThemes();

            QSpinBox *spin = editor->findChild<QSpinBox *>();
            spin->setValue(spin->value() + 1); // an edit
            editor->onApplyToAll();
            afterFirstEdit = derivedThemes();
            shownAfterEdit = editor->selectedThemeId();

            spin->setValue(spin->value() + 1); // a further edit
            editor->onApplyToAll();
            afterSecondEdit = derivedThemes();
            dlg->reject();
        });
        driver.start(50);
        QVERIFY(QMetaObject::invokeMethod(&window, "onOpenSettings"));

        QCOMPARE(afterUntouched.size(), before);
        QCOMPARE(afterFirstEdit.size(), before + 1);
        QVERIFY(afterFirstEdit.contains(shownAfterEdit)); // the editor follows the new theme
        QCOMPARE(afterSecondEdit, afterFirstEdit);        // updated in place
        QVERIFY(TerminalThemeManager::instance().theme(QStringLiteral("classic_green")).builtin);
    }
};

QTEST_MAIN(TestThemeEditorApply)
#include "test_theme_editor_apply.moc"
