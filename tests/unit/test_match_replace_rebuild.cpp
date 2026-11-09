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
#include <QSignalSpy>
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;

// Issue #460: the Match & Replace overlay was rebuilt for the whole screen
// on every written cell. It is now rebuilt once, before the next paint.
class TestMatchReplaceRebuild : public QObject {
    Q_OBJECT

  private slots:
    void fullScreenWriteRebuildsOnce() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.resize(800, 480);
        core::MatchReplaceEngine engine;
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("AB+C");
        rule.replacement = QStringLiteral("xyz");
        rule.isRegex = true;
        engine.setRules({rule});
        engine.setEnabled(true);
        widget.setMatchReplaceEngine(&engine);
        QSignalSpy rebuilds(&engine, &core::MatchReplaceEngine::overlayRebuilt);

        // A host record drawing a full screen writes every cell
        auto *buf = widget.screenBuffer();
        const QString row = QStringLiteral("abbbc ").repeated(14).left(80);
        for (int r = 0; r < 24; ++r)
            for (int c = 0; c < 80; ++c)
                buf->writeChar(r, c, core::EBCDIC::charToEBCDIC(row[c]));
        QCOMPARE(rebuilds.count(), 0);

        // The paint rebuilds it once, and the overlay is correct
        widget.grab();
        QCOMPARE(rebuilds.count(), 1);
        QVERIFY(engine.hasOverlay(10, 0));
        QCOMPARE(engine.overlayChar(10, 0), QChar('x'));
        QCOMPARE(engine.overlayChar(23, 2), QChar('z'));

        // No change, no rebuild
        widget.grab();
        QCOMPARE(rebuilds.count(), 1);
    }

    // Rules compiled in setRules() follow later setRules() calls
    void newRulesApplyAfterSetRules() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        auto *buf = widget.screenBuffer();
        for (int i = 0; i < 5; ++i)
            buf->writeChar(0, i, core::EBCDIC::charToEBCDIC(QStringLiteral("hello")[i]));
        core::MatchReplaceEngine engine;
        core::MatchReplaceRule rule;
        rule.pattern = QStringLiteral("h(e)llo");
        rule.replacement = QStringLiteral("J\\1LLO");
        rule.isRegex = true;
        engine.setRules({rule});
        engine.setEnabled(true);
        widget.setMatchReplaceEngine(&engine);
        widget.refreshMatchReplaceOverlay();
        QCOMPARE(engine.overlayChar(0, 0), QChar('J'));
        QVERIFY(!engine.hasOverlay(0, 1)); // 'e' is kept by the \1 group
        QCOMPARE(engine.overlayChar(0, 2), QChar('L'));
        rule.pattern = QStringLiteral("HEL+O");
        rule.replacement = QStringLiteral("bye");
        rule.caseSensitive = true;
        engine.setRules({rule});
        widget.refreshMatchReplaceOverlay();
        QVERIFY(!engine.hasOverlay(0, 0)); // case-sensitive now: no match
    }
};

QTEST_MAIN(TestMatchReplaceRebuild)
#include "test_match_replace_rebuild.moc"
