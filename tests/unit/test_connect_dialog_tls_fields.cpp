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
#include "ui/dialogs/connect_dialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QtTest/QtTest>

// Regression tests for issue #410: the TLS certificate fields stayed editable
// with "Use TLS/SSL" off.
class TestConnectDialogTlsFields : public QObject {
    Q_OBJECT

    static QCheckBox *tlsCheck(ConnectDialog &dialog) {
        for (QCheckBox *c : dialog.findChildren<QCheckBox *>())
            if (c->text() == QStringLiteral("Use TLS/SSL")) return c;
        return nullptr;
    }

    static QList<QLineEdit *> tlsEdits(ConnectDialog &dialog) {
        static const QStringList placeholders = {QStringLiteral("(system store only)"),
                                                 QStringLiteral("(none)"),
                                                 QStringLiteral("(read from the certificate file)")};
        QList<QLineEdit *> edits;
        for (QLineEdit *e : dialog.findChildren<QLineEdit *>())
            if (placeholders.contains(e->placeholderText())) edits << e;
        return edits;
    }

    static int enabledBrowseButtons(ConnectDialog &dialog) {
        int n = 0;
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Browse...") && b->isEnabled()) ++n;
        return n;
    }

    static bool allEnabled(const QList<QLineEdit *> &edits, bool enabled) {
        for (QLineEdit *e : edits)
            if (e->isEnabled() != enabled) return false;
        return true;
    }

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void fieldsFollowTheCheckbox() {
        ConnectDialog dialog;
        QCheckBox *tls = tlsCheck(dialog);
        QVERIFY(tls);
        const QList<QLineEdit *> edits = tlsEdits(dialog);
        QCOMPARE(edits.size(), 3);

        QCheckBox *allowInvalid = nullptr;
        for (QCheckBox *c : dialog.findChildren<QCheckBox *>())
            if (c->text().startsWith(QStringLiteral("Allow invalid TLS certificates"))) allowInvalid = c;
        QVERIFY(allowInvalid);

        QVERIFY(!tls->isChecked());
        QVERIFY2(allEnabled(edits, false), "certificate fields enabled with TLS off");
        QVERIFY(!allowInvalid->isEnabled());
        const int browseOff = enabledBrowseButtons(dialog);

        tls->setChecked(true);
        QVERIFY(allEnabled(edits, true));
        QVERIFY(allowInvalid->isEnabled());
        QCOMPARE(enabledBrowseButtons(dialog), browseOff + 3);

        tls->setChecked(false);
        QVERIFY(allEnabled(edits, false));
        QCOMPARE(enabledBrowseButtons(dialog), browseOff);
    }

    void fieldsFollowALoadedSession() {
        session::SessionConfig config;
        config.setName(QStringLiteral("tls"));
        config.setHostname(QStringLiteral("ibmi.example"));
        config.setUseTLS(true);
        config.setTlsCaCertificateFile(QStringLiteral("/tmp/ca.pem"));

        ConnectDialog dialog;
        dialog.setSessionConfig(config);
        QVERIFY(allEnabled(tlsEdits(dialog), true));

        config.setUseTLS(false);
        dialog.setSessionConfig(config);
        QVERIFY(allEnabled(tlsEdits(dialog), false));
        // A disabled path is still kept with the session
        QCOMPARE(dialog.getSessionConfig().tlsCaCertificateFile(), QStringLiteral("/tmp/ca.pem"));
    }
};

QTEST_MAIN(TestConnectDialogTlsFields)
#include "test_connect_dialog_tls_fields.moc"
