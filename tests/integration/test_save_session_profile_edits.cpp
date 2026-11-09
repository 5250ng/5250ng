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
#include "session/manager.h"
#include "ui/dialogs/session_settings_dialog.h"
#include "ui/dialogs/settings_dialog.h"
#include "ui/main_window.h"
#include "ui/themes/terminal_theme_manager.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

// Regression tests for issue #348: Session > Save wrote the tab's copy of the
// profile over edits made to the saved profile (Connect dialog, another tab)
// after the tab opened, without telling the user.
class TestSaveSessionProfileEdits : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        // As main() does: Quick Theme applies one of these
        ui::themes::TerminalThemeManager::instance().loadBuiltinThemes();
    }
    void init();
    void cleanup();

    void saveAsksBeforeReplacingEditedProfile();
    void editBeforeQuickThemeStillAsks();
    void settingsApplyToAllSavesTheme();
    // Last: a reopened tab's teardown is still pending when its window goes
    void editBeforeReconnectStillAsks();

  private:
    void save();
    void editProfile();
    QString savedUser() const;

    const QString m_name = QStringLiteral("profile-edits-test");
    QTcpServer *m_server = nullptr;
    int m_connections = 0;
    MainWindow *m_window = nullptr;
    QTimer *m_responder = nullptr;
    QString m_answer;          // button clicked on a Yes/No question
    QStringList m_questions;   // text of each question asked
};

void TestSaveSessionProfileEdits::init() {
    m_server = new QTcpServer(this);
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
    m_connections = 0;
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket *socket = m_server->nextPendingConnection()) {
            ++m_connections;
            socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
        }
    });

    session::SessionConfig config;
    config.setName(m_name);
    config.setHostname(QStringLiteral("127.0.0.1"));
    config.setPort(m_server->serverPort());
    config.setUsername(QStringLiteral("ORIGINAL"));
    session::SessionManager mgr;
    QVERIFY(mgr.saveSession(config));

    m_window = new MainWindow();
    m_window->autoConnect(config);
    QTRY_COMPARE_WITH_TIMEOUT(m_connections, 1, 15000);

    // Answer every message box: questions with m_answer, the rest with OK
    m_answer = QStringLiteral("No");
    m_questions.clear();
    m_responder = new QTimer(this);
    connect(m_responder, &QTimer::timeout, this, [this]() {
        QWidget *modal = QApplication::activeModalWidget();
        if (!modal) return;
        const auto buttons = modal->findChildren<QPushButton *>();
        bool isQuestion = false;
        for (QPushButton *b : buttons)
            if (b->text().remove('&') == QStringLiteral("No")) isQuestion = true;
        if (isQuestion) {
            QString text;
            for (QLabel *l : modal->findChildren<QLabel *>()) text += l->text();
            m_questions << text;
        }
        for (QPushButton *b : buttons) {
            const QString t = b->text().remove('&');
            if ((isQuestion && t == m_answer) || (!isQuestion && t == QStringLiteral("OK"))) {
                b->click();
                return;
            }
        }
    });
    m_responder->start(20);
}

void TestSaveSessionProfileEdits::cleanup() {
    m_responder->stop();
    // Let closed tabs finish their deferred teardown before the window goes
    QTest::qWait(200);
    delete m_window;
    m_window = nullptr;
    delete m_responder;
    delete m_server;
    session::SessionManager().deleteSession(m_name);
}

void TestSaveSessionProfileEdits::save() {
    QVERIFY(QMetaObject::invokeMethod(m_window, "onSaveSession"));
    QTRY_VERIFY_WITH_TIMEOUT(!QApplication::activeModalWidget(), 5000);
}

// Edits the saved profile behind the tab's back, as the Connect dialog does
void TestSaveSessionProfileEdits::editProfile() {
    session::SessionManager mgr;
    session::SessionConfig edited;
    QVERIFY(mgr.loadSession(m_name, edited));
    edited.setUsername(QStringLiteral("EDITED"));
    QVERIFY(mgr.saveSession(edited));
}

QString TestSaveSessionProfileEdits::savedUser() const {
    session::SessionManager reader;
    session::SessionConfig saved;
    return reader.loadSession(m_name, saved) ? saved.username() : QString();
}

void TestSaveSessionProfileEdits::saveAsksBeforeReplacingEditedProfile() {
    // Unchanged profile: Save writes without asking
    save();
    QCOMPARE(m_questions.size(), 0);

    // Declining keeps the edit
    editProfile();
    save();
    QCOMPARE(m_questions.size(), 1);
    QVERIFY2(m_questions.at(0).contains(m_name), qPrintable(m_questions.at(0)));
    QCOMPARE(savedUser(), QStringLiteral("EDITED"));

    // Confirming replaces it, and a later Save does not ask again
    m_answer = QStringLiteral("Yes");
    save();
    QCOMPARE(m_questions.size(), 2);
    QCOMPARE(savedUser(), QStringLiteral("ORIGINAL"));
    save();
    QCOMPARE(m_questions.size(), 2);
}

void TestSaveSessionProfileEdits::editBeforeReconnectStillAsks() {
    // Reconnect reopens the tab with its old copy; the edit made before it
    // must still be noticed
    editProfile();
    QVERIFY(QMetaObject::invokeMethod(m_window, "onReconnect"));
    QTRY_COMPARE_WITH_TIMEOUT(m_connections, 2, 15000);
    save();
    QCOMPARE(m_questions.size(), 1);
    QCOMPARE(savedUser(), QStringLiteral("EDITED"));
}

void TestSaveSessionProfileEdits::editBeforeQuickThemeStillAsks() {
    // Quick Theme writes only the theme into the edited profile; the tab's
    // copy still lacks the edit, so Save must still ask
    editProfile();
    QAction theme;
    theme.setData(QStringLiteral("classic_green"));
    QVERIFY(QMetaObject::invokeMethod(m_window, "onQuickThemeChosen", Q_ARG(QAction *, &theme)));
    save();
    QCOMPARE(m_questions.size(), 1);
    QCOMPARE(savedUser(), QStringLiteral("EDITED"));
}

// Issue #472: Settings > 5250 Theme, applied to all sessions, wrote the
// theme only to the tabs, not to their saved profiles; and the write is the
// tab's own, so Save must not ask about it
void TestSaveSessionProfileEdits::settingsApplyToAllSavesTheme() {
    auto savedTheme = [this]() {
        session::SessionManager reader;
        session::SessionConfig saved;
        return reader.loadSession(m_name, saved) ? saved.terminalThemeId() : QStringLiteral("(missing)");
    };
    const QString before = savedTheme();

    m_responder->stop(); // the Settings dialog is driven below instead
    QString applied;
    QTimer driver;
    connect(&driver, &QTimer::timeout, this, [&]() {
        auto *dlg = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        if (!dlg) return;
        driver.stop();
        SessionSettingsDialog *editor = dlg->terminalThemeEditor();
        QSpinBox *spin = editor->findChild<QSpinBox *>();
        spin->setValue(spin->value() + 1); // an edit gives a new derived theme
        editor->onApplyToAll();
        applied = editor->selectedThemeId();
        dlg->reject();
    });
    driver.start(50);
    QVERIFY(QMetaObject::invokeMethod(m_window, "onOpenSettings"));
    m_responder->start(20);

    QVERIFY(!applied.isEmpty());
    QVERIFY(applied != before);
    QCOMPARE(savedTheme(), applied);
    save();
    QCOMPARE(m_questions.size(), 0);
}

QTEST_MAIN(TestSaveSessionProfileEdits)
#include "test_save_session_profile_edits.moc"
