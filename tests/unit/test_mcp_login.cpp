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
#include "session/manager.h"
#include "ui/dialogs/mcp_password_prompt_dialog.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QLineEdit>
#include <QTimer>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

class TestMcpLogin : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void init();
    void cleanup();
    void testMissingPasswordWithoutPromptFails();
    void testCancelledPromptTypesNothing();
    void testPromptedPasswordIsTypedButNotReturned();
    void testExplicitPasswordSkipsPrompt();
    void testPromptDialogMasksAndReturnsPassword();
    void testPromptDialogTimesOut();
    void testListSavedSessionsShowsOnlyMcpAllowed();
    void testCreateSessionRejectsUnavailableProfile_data();
    void testCreateSessionRejectsUnavailableProfile();
    void testCreateSessionFromProfileUsesSavedSettings();
    void testLoginUsesSavedCredentials();
    void testLoginAsOtherUserPrompts();
    void testDefaultProviderReadsSavedSessions();

  private:
    // Minimal IBM i sign-on screen: "Sign On" on row 1, a user field and a
    // password field, keyboard unlocked.
    void showSignOnScreen();
    QString fieldText(int row, int col) const;
    QJsonObject login(const QJsonObject &extra);
    static QString resultText(const QJsonObject &result);
    // Saved sessions: PROD is allowed for MCP and has credentials, DEV is not
    // allowed.
    void installSavedSessions();
    // Runs create_session with the given arguments, standing in for
    // MainWindow by attaching m_widget to the new session.
    QJsonObject createSession(const QJsonObject &args, QString *sessionId = nullptr);

    session::SessionConfig m_requestedConfig;
    int m_createRequests = 0;

    Q5250ScreenWidget *m_widget = nullptr;
    mcp::McpSessionRegistry *m_registry = nullptr;
    mcp::McpToolHandler *m_handler = nullptr;
    QByteArray m_sentToHost;
};

static const QString kSessionId = QStringLiteral("test-session");

void TestMcpLogin::initTestCase() {
    // Keep SessionManager away from the user's real saved sessions.
    QStandardPaths::setTestModeEnabled(true);
}

void TestMcpLogin::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_registry = new mcp::McpSessionRegistry();
    m_handler = new mcp::McpToolHandler(m_registry);
    mcp::McpSessionInfo info;
    info.sessionId = kSessionId;
    info.hostname = QStringLiteral("ibmi.example");
    m_registry->addSession(info);
    m_registry->setDisplayWidget(kSessionId, m_widget);
    m_sentToHost.clear();
    // Stand in for the host: record the AID response, then unlock the
    // keyboard on the same (non-sign-on) screen so the login script ends.
    connect(m_widget, &Q5250ScreenWidget::inputReady, this, [this](const QByteArray &data) {
        m_sentToHost += data;
        QTimer::singleShot(0, this, [this]() {
            m_widget->setKeyboardState(KeyboardState::Unlocked);
        });
    });
}

void TestMcpLogin::cleanup() {
    delete m_handler;
    delete m_registry;
    delete m_widget;
}

void TestMcpLogin::showSignOnScreen() {
    auto *screen = m_widget->screenBuffer();
    const QString title = QStringLiteral("Sign On");
    for (int i = 0; i < title.size(); ++i)
        screen->writeChar(0, 35 + i, core::EBCDIC::charToEBCDIC(title[i]));
    screen->setField(5, 52, 10, false);
    screen->setFieldFFW(5, 52, 0x40, 0x00);
    screen->setField(6, 52, 10, false);
    screen->setFieldFFW(6, 52, 0x40, 0x00);
    m_widget->setKeyboardState(KeyboardState::Unlocked);
}

QString TestMcpLogin::fieldText(int row, int col) const {
    auto *screen = m_widget->screenBuffer();
    return core::EBCDIC::ebcdicToString(screen->getFieldData(screen->getField(row, col)))
        .remove(QChar(0)).trimmed();
}

QJsonObject TestMcpLogin::login(const QJsonObject &extra) {
    QJsonObject args = extra;
    args["session_id"] = kSessionId;
    args["username"] = QStringLiteral("QSECOFR");
    return m_handler->callTool(QStringLiteral("login"), args);
}

QString TestMcpLogin::resultText(const QJsonObject &result) {
    return result.value("content").toArray().at(0).toObject().value("text").toString();
}

void TestMcpLogin::testMissingPasswordWithoutPromptFails() {
    showSignOnScreen();
    QJsonObject result = login({});
    QVERIFY(result.value("isError").toBool());
    QVERIFY(resultText(result).contains("password prompt"));
    QVERIFY(fieldText(5, 52).isEmpty());
}

void TestMcpLogin::testCancelledPromptTypesNothing() {
    showSignOnScreen();
    QString seenSession, seenHost, seenUser;
    m_handler->setPasswordPrompt([&](const QString &sessionId, const QString &hostname,
                                     const QString &username, QString *) {
        seenSession = sessionId;
        seenHost = hostname;
        seenUser = username;
        return false;
    });

    QJsonObject result = login({});
    QVERIFY(result.value("isError").toBool());
    QVERIFY(resultText(result).contains("cancelled"));
    QCOMPARE(seenSession, kSessionId);
    QCOMPARE(seenHost, QStringLiteral("ibmi.example"));
    QCOMPARE(seenUser, QStringLiteral("QSECOFR"));
    QVERIFY(fieldText(5, 52).isEmpty());
    QVERIFY(m_sentToHost.isEmpty());
}

// The prompted password reaches the host but never the MCP result.
void TestMcpLogin::testPromptedPasswordIsTypedButNotReturned() {
    showSignOnScreen();
    const QString secret = QStringLiteral("S3CRETPW");
    int prompts = 0;
    m_handler->setPasswordPrompt([&](const QString &, const QString &, const QString &,
                                     QString *password) {
        ++prompts;
        *password = secret;
        return true;
    });

    QJsonObject result = login({});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(prompts, 1);
    QVERIFY(!QJsonDocument(result).toJson().contains(secret.toUtf8()));
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC(secret)));
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("QSECOFR")));
}

void TestMcpLogin::testExplicitPasswordSkipsPrompt() {
    showSignOnScreen();
    int prompts = 0;
    m_handler->setPasswordPrompt([&](const QString &, const QString &, const QString &,
                                     QString *) {
        ++prompts;
        return false;
    });

    QJsonObject result = login({{"password", "GIVENPW"}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(prompts, 0);
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("GIVENPW")));
}

void TestMcpLogin::testPromptDialogMasksAndReturnsPassword() {
    QString password;
    QTimer::singleShot(0, this, []() {
        auto *dlg = qobject_cast<ui::dialogs::McpPasswordPromptDialog *>(
            QApplication::activeModalWidget());
        QVERIFY(dlg);
        auto *edit = dlg->findChild<QLineEdit *>();
        QVERIFY(edit);
        QCOMPARE(edit->echoMode(), QLineEdit::Password);
        edit->setText(QStringLiteral("TYPEDPW"));
        dlg->accept();
    });
    QVERIFY(ui::dialogs::McpPasswordPromptDialog::ask(
        "ibmi.example", "QSECOFR", &password, nullptr, 60000));
    QCOMPARE(password, QStringLiteral("TYPEDPW"));
}

void TestMcpLogin::testPromptDialogTimesOut() {
    QString password = QStringLiteral("unchanged");
    QVERIFY(!ui::dialogs::McpPasswordPromptDialog::ask(
        "ibmi.example", "QSECOFR", &password, nullptr, 50));
    QCOMPARE(password, QStringLiteral("unchanged"));
}

void TestMcpLogin::installSavedSessions() {
    m_handler->setSavedSessionsProvider([]() {
        session::SessionConfig prod;
        prod.setName("PROD");
        prod.setHostname("prod.example");
        prod.setPort(992);
        prod.setUseTLS(true);
        prod.setDeviceType("IBM-3477-FC");
        prod.setUsername("AGENTUSR");
        prod.setPassword("SAVEDPW1");
        prod.setMcpAllowed(true);

        session::SessionConfig dev;
        dev.setName("DEV");
        dev.setHostname("dev.example");
        dev.setUsername("DEVUSR");
        dev.setPassword("DEVPW");
        return QList<session::SessionConfig>{prod, dev};
    });
}

QJsonObject TestMcpLogin::createSession(const QJsonObject &args, QString *sessionId) {
    m_createRequests = 0;
    auto conn = connect(m_handler, &mcp::McpToolHandler::createSessionRequested, this,
                        [this](const QString &id, const session::SessionConfig &config) {
        ++m_createRequests;
        m_requestedConfig = config;
        m_handler->onSessionCreated(id, m_widget);
    });
    QJsonObject result = m_handler->callTool(QStringLiteral("create_session"), args);
    disconnect(conn);
    if (sessionId) {
        static const QRegularExpression idRe(QStringLiteral("session_id: (\\S+)"));
        *sessionId = idRe.match(resultText(result)).captured(1);
    }
    return result;
}

void TestMcpLogin::testListSavedSessionsShowsOnlyMcpAllowed() {
    installSavedSessions();
    QJsonObject result = m_handler->callTool(QStringLiteral("list_saved_sessions"), {});
    const QString text = resultText(result);
    QVERIFY(!result.value("isError").toBool());
    QVERIFY(text.contains("profile: PROD  host: prod.example:992  tls: yes  saved credentials: yes"));
    QVERIFY(!text.contains("DEV"));
    QVERIFY(!text.contains("SAVEDPW1"));
    QVERIFY(!text.contains("AGENTUSR"));
}

void TestMcpLogin::testCreateSessionRejectsUnavailableProfile_data() {
    QTest::addColumn<QJsonObject>("args");
    QTest::newRow("not allowed") << QJsonObject{{"profile", "DEV"}};
    QTest::newRow("unknown") << QJsonObject{{"profile", "NOPE"}};
    QTest::newRow("hostname and profile")
        << QJsonObject{{"profile", "PROD"}, {"hostname", "other.example"}};
}

void TestMcpLogin::testCreateSessionRejectsUnavailableProfile() {
    QFETCH(QJsonObject, args);
    installSavedSessions();
    QJsonObject result = createSession(args);
    QVERIFY(result.value("isError").toBool());
    QCOMPARE(m_createRequests, 0);
}

// The tab is opened with the saved session's settings, credentials
// included (for encrypted sign-on), but none of them are returned.
void TestMcpLogin::testCreateSessionFromProfileUsesSavedSettings() {
    installSavedSessions();
    QJsonObject result = createSession({{"profile", "PROD"}, {"port", 23}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(m_createRequests, 1);
    QCOMPARE(m_requestedConfig.name(), QStringLiteral("PROD"));
    QCOMPARE(m_requestedConfig.hostname(), QStringLiteral("prod.example"));
    QCOMPARE(m_requestedConfig.port(), quint16(992));
    QVERIFY(m_requestedConfig.useTLS());
    QCOMPARE(m_requestedConfig.deviceType(), QStringLiteral("IBM-3477-FC"));
    QCOMPARE(m_requestedConfig.username(), QStringLiteral("AGENTUSR"));
    QCOMPARE(m_requestedConfig.password(), QStringLiteral("SAVEDPW1"));
    QVERIFY(!QJsonDocument(result).toJson().contains("SAVEDPW1"));
}

void TestMcpLogin::testLoginUsesSavedCredentials() {
    installSavedSessions();
    QString sessionId;
    QVERIFY(!createSession({{"profile", "PROD"}}, &sessionId).value("isError").toBool());
    int prompts = 0;
    m_handler->setPasswordPrompt([&](const QString &, const QString &, const QString &,
                                     QString *) {
        ++prompts;
        return false;
    });
    showSignOnScreen();

    QJsonObject result = m_handler->callTool(QStringLiteral("login"),
                                             {{"session_id", sessionId}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(prompts, 0);
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("AGENTUSR")));
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("SAVEDPW1")));
    QVERIFY(!QJsonDocument(result).toJson().contains("SAVEDPW1"));
}

// Another user profile never gets the saved password.
void TestMcpLogin::testLoginAsOtherUserPrompts() {
    installSavedSessions();
    QString sessionId;
    QVERIFY(!createSession({{"profile", "PROD"}}, &sessionId).value("isError").toBool());
    QString promptedUser;
    m_handler->setPasswordPrompt([&](const QString &, const QString &,
                                     const QString &username, QString *) {
        promptedUser = username;
        return false;
    });
    showSignOnScreen();

    QJsonObject result = m_handler->callTool(QStringLiteral("login"),
                                             {{"session_id", sessionId}, {"username", "OTHER"}});
    QVERIFY(result.value("isError").toBool());
    QCOMPARE(promptedUser, QStringLiteral("OTHER"));
    QVERIFY(!m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("SAVEDPW1")));
}

// Without an injected provider, saved sessions come from SessionManager.
void TestMcpLogin::testDefaultProviderReadsSavedSessions() {
    session::SessionManager manager;
    session::SessionConfig allowed;
    allowed.setName("MCP default provider allowed");
    allowed.setHostname("allowed.example");
    allowed.setMcpAllowed(true);
    session::SessionConfig hidden;
    hidden.setName("MCP default provider hidden");
    hidden.setHostname("hidden.example");
    QVERIFY(manager.saveSession(allowed));
    QVERIFY(manager.saveSession(hidden));

    const QString text = resultText(
        m_handler->callTool(QStringLiteral("list_saved_sessions"), {}));
    manager.deleteSession(allowed.name());
    manager.deleteSession(hidden.name());
    QVERIFY2(text.contains("profile: MCP default provider allowed  host: allowed.example:23"),
             qPrintable(text));
    QVERIFY(!text.contains("hidden.example"));
}

QTEST_MAIN(TestMcpLogin)
#include "test_mcp_login.moc"
