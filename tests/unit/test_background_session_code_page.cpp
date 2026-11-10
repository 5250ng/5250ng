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

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Issue #482: work done for a session that is not the active tab (typed or
// injected keys, history snapshots, the Match & Replace overlay) used the
// process-wide code page, which is the active tab's. Each session has its
// own. The active tab is CP037 here; the session is CP273, where bytes
// 4A 5A C0 are A, U and a with umlaut (CP037: cent, !, left brace;
// Python's cp037/cp273 codecs agree).
class TestBackgroundSessionCodePage : public QObject {
    Q_OBJECT

    const QString m_text = QString(QChar(0x00C4)) + QChar(0x00DC) + QChar(0x00E4);

    void writeText(Q5250ScreenWidget &widget) {
        auto *buf = widget.screenBuffer();
        buf->writeChar(3, 10, 0x4A);
        buf->writeChar(3, 11, 0x5A);
        buf->writeChar(3, 12, 0xC0);
    }

  private slots:
    void init() { core::EBCDIC::setCodePage(core::CodePage::ID::CP037); }

    void typedCharacterUsesSessionPage() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setCodePage(core::CodePage::ID::CP273);
        auto *buf = widget.screenBuffer();
        buf->setField(2, 10, 5, false);
        buf->setCursorPosition(2, 10);
        widget.setKeyboardState(KeyboardState::Unlocked);
        widget.dispatchCharacter(QChar(0x00C4));
        QCOMPARE(int(buf->character(2, 10)), 0x4A);
    }

    void historySnapshotUsesSessionPage() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setCodePage(core::CodePage::ID::CP273);
        writeText(widget);
        QTRY_VERIFY_WITH_TIMEOUT(widget.screenHistory()->count() > 0, 3000);
        const QString snapshot = QString::fromUtf8(widget.screenHistory()->at(0).cellData);
        QVERIFY2(snapshot.contains(m_text), qPrintable(snapshot.simplified()));
    }

    void matchReplaceOverlayUsesSessionPage() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setCodePage(core::CodePage::ID::CP273);
        writeText(widget);
        core::MatchReplaceEngine engine;
        core::MatchReplaceRule rule;
        rule.pattern = m_text;
        rule.replacement = QStringLiteral("XYZ");
        rule.caseSensitive = true;
        engine.setRules({rule});
        engine.setEnabled(true);
        widget.setMatchReplaceEngine(&engine);
        widget.refreshMatchReplaceOverlay();
        QVERIFY(engine.hasOverlay(3, 10));
        QCOMPARE(engine.overlayChar(3, 10), QChar('X'));
    }
};

QTEST_MAIN(TestBackgroundSessionCodePage)
#include "test_background_session_code_page.moc"
