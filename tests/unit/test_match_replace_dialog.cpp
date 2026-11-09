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

#include "ui/dialogs/match_replace_dialog.h"
#include <QLineEdit>
#include <QTableWidget>
#include <QtTest/QtTest>

using ui::dialogs::MatchReplaceDialog;

// Issue #461: a regex rule whose pattern does not compile used to be
// accepted and then never matched, with no feedback.
class TestMatchReplaceDialog : public QObject {
    Q_OBJECT

  private slots:
    void invalidRegexKeepsDialogOpen() {
        core::MatchReplaceRule good;
        good.pattern = QStringLiteral("CPF\\d{4}");
        good.isRegex = true;
        core::MatchReplaceRule bad;
        bad.pattern = QStringLiteral("ERR(");
        bad.isRegex = true;
        MatchReplaceDialog dialog({good, bad});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY2(dialog.errorText().startsWith("Rule 2: the regular expression is not valid:"),
                 qPrintable(dialog.errorText()));
        QVERIFY2(dialog.errorText().contains("position 5"), qPrintable(dialog.errorText()));

        // Fixing the pattern lets the dialog close
        auto *table = dialog.findChild<QTableWidget *>();
        QVERIFY(table);
        table->item(1, 1)->setText(QStringLiteral("ERR\\("));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.rules().at(1).pattern, QStringLiteral("ERR\\("));
    }

    // A pattern typed into a cell editor that is still open is checked too
    void openEditorIsCommittedFirst() {
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("B");
        rule.isRegex = true;
        MatchReplaceDialog dialog({rule});
        dialog.show();
        auto *table = dialog.findChild<QTableWidget *>();
        QVERIFY(table);
        table->setCurrentCell(0, 1);
        table->editItem(table->item(0, 1));
        auto *editor = table->findChild<QLineEdit *>();
        QVERIFY(editor);
        editor->setText(QStringLiteral("ERR("));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("ERR("));
        QVERIFY(table->selectedItems().isEmpty());
    }

    // The same text as plain text is fine: it is matched literally
    void plainTextRuleIsNotValidated() {
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("ERR(");
        MatchReplaceDialog dialog({rule});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(MatchReplaceDialog::patternError(rule).isEmpty());
    }

    void patternErrorReportsQtMessage() {
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("[a-");
        rule.isRegex = true;
        const QString error = MatchReplaceDialog::patternError(rule);
        QVERIFY(!error.isEmpty());
        QVERIFY2(error.contains(QRegularExpression(rule.pattern).errorString()), qPrintable(error));
    }
};

QTEST_MAIN(TestMatchReplaceDialog)
#include "test_match_replace_dialog.moc"
