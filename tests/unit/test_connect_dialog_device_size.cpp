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
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTimer>
#include <QtTest/QtTest>

// Regression tests for issue #411: a custom device type naming a known model
// was accepted with a screen size that model cannot show.
class TestConnectDialogDeviceSize : public QObject {
    Q_OBJECT

    // Close the next modal dialog with OK, recording its text
    void answerNextWarning() {
        m_warning.clear();
        QTimer::singleShot(50, this, [this] { tryAnswer(0); });
    }

    void tryAnswer(int attempt) {
        QWidget *modal = QApplication::activeModalWidget();
        // The ConnectDialog itself is not modal here, so any modal is the warning
        if (!modal) {
            if (attempt < 100) QTimer::singleShot(20, this, [this, attempt] { tryAnswer(attempt + 1); });
            return;
        }
        for (QLabel *l : modal->findChildren<QLabel *>())
            m_warning += l->text() + '\n';
        bool clicked = false;
        for (QPushButton *b : modal->findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("OK")) {
                b->click();
                clicked = true;
                break;
            }
        }
        if (!clicked) modal->close();
        // Keep closing dialogs for a while, so a regression that opens more
        // than the warning (a name prompt, then a "saved" box) fails instead
        // of hanging
        if (attempt < 100) QTimer::singleShot(20, this, [this, attempt] { tryAnswer(attempt + 1); });
    }

    // Load a config the Connect dialog shows as a custom device, then press Connect
    bool connectWith(const QString &deviceType, int rows, int cols) {
        session::SessionConfig config;
        config.setName(QStringLiteral("size"));
        config.setHostname(QStringLiteral("ibmi.example"));
        config.setDeviceType(deviceType);
        config.setScreenRows(rows);
        config.setScreenCols(cols);

        ConnectDialog dialog;
        dialog.setSessionConfig(config);
        QSignalSpy requested(&dialog, &ConnectDialog::connectRequested);
        QPushButton *connectButton = nullptr;
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Connect")) connectButton = b;
        if (!connectButton) return false;
        answerNextWarning();
        connectButton->click();
        return requested.size() == 1;
    }

    QString m_warning;

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void tooLargeForTheModelIsRefused() {
        // Lower case so the device list does not select the model, as a
        // hand-edited session file would
        QVERIFY(!connectWith(QStringLiteral("ibm-3179-2"), 27, 132));
        QVERIFY2(m_warning.contains(QStringLiteral("IBM-3179-2 supports a screen of at most 24x80")),
                 qPrintable(m_warning));
        QVERIFY(!connectWith(QStringLiteral("ibm-3179-2"), 24, 132));
        QVERIFY2(m_warning.contains(QStringLiteral("the screen size is 24x132")), qPrintable(m_warning));
    }

    void saveAsRefusesTooLargeASize() {
        session::SessionConfig config;
        config.setName(QStringLiteral("size"));
        config.setHostname(QStringLiteral("ibmi.example"));
        config.setDeviceType(QStringLiteral("ibm-3179-2"));
        config.setScreenRows(27);
        config.setScreenCols(132);

        ConnectDialog dialog;
        dialog.setSessionConfig(config);
        QPushButton *saveAs = nullptr;
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Save As...")) saveAs = b;
        QVERIFY(saveAs);
        // Refused before the name prompt, so the only dialog is the warning
        answerNextWarning();
        saveAs->click();
        QVERIFY2(m_warning.contains(QStringLiteral("IBM-3179-2 supports a screen of at most 24x80")),
                 qPrintable(m_warning));
    }

    void sizesTheModelSupportsAreAccepted() {
        QVERIFY(connectWith(QStringLiteral("ibm-3179-2"), 24, 80));
        // A 27x132 display can also show a 24x80 screen
        QVERIFY(connectWith(QStringLiteral("ibm-3477-fc"), 24, 80));
        QVERIFY(connectWith(QStringLiteral("ibm-3477-fc"), 27, 132));
    }

    void unknownCustomDevicesAreNotChecked() {
        QVERIFY(connectWith(QStringLiteral("MY-DEVICE"), 27, 132));
    }
};

QTEST_MAIN(TestConnectDialogDeviceSize)
#include "test_connect_dialog_device_size.moc"
