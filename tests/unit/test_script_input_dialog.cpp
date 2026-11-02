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

#include "ui/dialogs/script_input_dialog.h"
#include <QApplication>
#include <QLineEdit>
#include <QTimer>
#include <QtTest/QtTest>

using ui::dialogs::ScriptInputDialog;

class TestScriptInputDialog : public QObject {
    Q_OBJECT

  private slots:
    void testPasswordFieldsAreMasked();
    void testAskReturnsValuesInOrder();
    void testAskCancelled();
};

void TestScriptInputDialog::testPasswordFieldsAreMasked() {
    ScriptInputDialog dlg({"User:", "Password:", "Library:"}, {false, true, false});
    const auto fields = dlg.findChildren<QLineEdit *>();
    QCOMPARE(fields.size(), 3);
    QCOMPARE(fields[0]->echoMode(), QLineEdit::Normal);
    QCOMPARE(fields[1]->echoMode(), QLineEdit::Password);
    QCOMPARE(fields[2]->echoMode(), QLineEdit::Normal);

    // A missing flag means a plain field
    ScriptInputDialog plain({"Name:"}, {});
    QCOMPARE(plain.findChild<QLineEdit *>()->echoMode(), QLineEdit::Normal);
}

void TestScriptInputDialog::testAskReturnsValuesInOrder() {
    QTimer::singleShot(0, this, []() {
        auto *dlg = qobject_cast<ScriptInputDialog *>(QApplication::activeModalWidget());
        QVERIFY(dlg);
        const auto fields = dlg->findChildren<QLineEdit *>();
        fields[0]->setText("BOB");
        fields[1]->setText("S3CRET");
        dlg->accept();
    });
    QStringList values;
    QVERIFY(ScriptInputDialog::ask({"User:", "Password:"}, {false, true}, &values, nullptr));
    QCOMPARE(values, (QStringList{"BOB", "S3CRET"}));
}

void TestScriptInputDialog::testAskCancelled() {
    QTimer::singleShot(0, this, []() {
        if (auto *dlg = qobject_cast<ScriptInputDialog *>(QApplication::activeModalWidget()))
            dlg->reject();
    });
    QStringList values{"unchanged"};
    QVERIFY(!ScriptInputDialog::ask({"Password:"}, {true}, &values, nullptr));
    QCOMPARE(values, QStringList{"unchanged"});
}

QTEST_MAIN(TestScriptInputDialog)
#include "test_script_input_dialog.moc"
