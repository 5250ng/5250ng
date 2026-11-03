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
#include "core/match_replace_engine.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;

// Match & Replace must never reveal what is in a non-display field (a
// password): those cells take part as blanks.
class TestMatchReplaceHiddenCells : public QObject {
    Q_OBJECT

  private slots:
    void hiddenCharactersStayHidden() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        auto *buf = widget.screenBuffer();
        const QString label = QStringLiteral("Password: ");
        const QString secret = QStringLiteral("S3CR3TXY");
        for (int i = 0; i < label.size(); ++i)
            buf->writeChar(5, i, core::EBCDIC::charToEBCDIC(label[i]));
        for (int i = 0; i < secret.size(); ++i) {
            const int col = label.size() + i;
            buf->writeChar(5, col, core::EBCDIC::charToEBCDIC(secret[i]));
            buf->cell(5, col).attributes.nonDisplay = true;
        }

        core::MatchReplaceEngine engine;
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("Password");
        rule.replacement = QStringLiteral("PW");
        engine.setRules({rule});
        engine.setEnabled(true);
        widget.setMatchReplaceEngine(&engine);
        widget.refreshMatchReplaceOverlay();

        // What the overlay shows on row 6 in cells that are displayed
        QString visible;
        for (int c = 0; c < 80; ++c) {
            if (buf->cell(5, c).attributes.nonDisplay)
                continue;
            visible += engine.hasOverlay(5, c) ? engine.overlayChar(5, c)
                                               : core::EBCDIC::ebcdicToChar(buf->character(5, c));
        }
        QVERIFY2(visible.startsWith("PW:"), qPrintable(visible));
        for (QChar ch : secret)
            if (ch.isLetter() || ch.isDigit())
                QVERIFY2(!visible.mid(3).contains(ch), qPrintable(visible));
    }

    // A rule that matches the hidden text itself does not fire
    void hiddenTextIsNotMatched() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        auto *buf = widget.screenBuffer();
        const QString secret = QStringLiteral("HUNTER2");
        for (int i = 0; i < secret.size(); ++i) {
            buf->writeChar(3, 10 + i, core::EBCDIC::charToEBCDIC(secret[i]));
            buf->cell(3, 10 + i).attributes.nonDisplay = true;
        }
        core::MatchReplaceEngine engine;
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("HUNTER2");
        rule.replacement = QStringLiteral("FOUND!!");
        engine.setRules({rule});
        engine.setEnabled(true);
        widget.setMatchReplaceEngine(&engine);
        widget.refreshMatchReplaceOverlay();
        for (int c = 0; c < 80; ++c)
            QVERIFY(!engine.hasOverlay(3, c) || engine.overlayChar(3, c) != QChar('F'));
    }
};

QTEST_MAIN(TestMatchReplaceHiddenCells)
#include "test_match_replace_hidden_cells.moc"
