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
#include "ui/dialogs/mcp_password_prompt_dialog.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QApplication>
#include <QLineEdit>
#include <QTimer>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

class TestMcpLogin : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void testMissingPasswordWithoutPromptFails();
    void testCancelledPromptTypesNothing();
    void testPromptedPasswordIsTypedButNotReturned();
    void testExplicitPasswordSkipsPrompt();
    void testPromptDialogMasksAndReturnsPassword();
    void testPromptDialogTimesOut();

  private:
    // Minimal IBM i sign-on screen: "Sign On" on row 1, a user field and a
    // password field, keyboard unlocked.
    void showSignOnScreen();
    QString fieldText(int row, int col) const;
    QJsonObject login(const QJsonObject &extra);
    static QString resultText(const QJsonObject &result);

    Q5250ScreenWidget *m_widget = nullptr;
    mcp::McpSessionRegistry *m_registry = nullptr;
    mcp::McpToolHandler *m_handler = nullptr;
    QByteArray m_sentToHost;
};

static const QString kSessionId = QStringLiteral("test-session");

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

QTEST_MAIN(TestMcpLogin)
#include "test_mcp_login.moc"
