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
#include <QStandardPaths>
#include <QtTest/QtTest>

// Issue #462: the connect dialog has no Match & Replace field, so it must
// carry the loaded session's rules through, or Connect and Save drop them.
class TestConnectDialogMatchReplace : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void rulesSurviveTheDialog() {
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("CPF(\\d{4})");
        rule.replacement = QStringLiteral("Error \\1");
        rule.isRegex = true;
        session::SessionConfig config;
        config.setName(QStringLiteral("rules"));
        config.setHostname(QStringLiteral("ibmi.example"));
        config.setMatchReplaceRules({rule});
        config.setMatchReplaceEnabled(true);

        ConnectDialog dialog;
        dialog.setSessionConfig(config);
        const session::SessionConfig out = dialog.getSessionConfig();
        QCOMPARE(out.matchReplaceEnabled(), true);
        QCOMPARE(out.matchReplaceRules().size(), 1);
        QCOMPARE(out.matchReplaceRules()[0].pattern, rule.pattern);
        QCOMPARE(out.matchReplaceRules()[0].replacement, rule.replacement);
        QCOMPARE(out.matchReplaceRules()[0].isRegex, true);
    }
};

QTEST_MAIN(TestConnectDialogMatchReplace)
#include "test_connect_dialog_match_replace.moc"
