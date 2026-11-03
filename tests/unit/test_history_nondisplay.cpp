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
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;
using ui::widgets::ScreenCell;

// Regression test for issue #383: screen-history snapshots stored the
// characters of non-display cells, so a typed password stayed in memory
// and was found by history search.
class TestHistoryNonDisplay : public QObject {
    Q_OBJECT

  private slots:
    void snapshotBlanksNonDisplayCells() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        auto *history = widget.screenHistory();
        history->clear();
        auto *buf = widget.screenBuffer();
        const QString label = QStringLiteral("Password");
        for (int i = 0; i < label.size(); ++i)
            buf->writeChar(0, i, core::EBCDIC::charToEBCDIC(label[i]));
        ui::widgets::CellAttributes hidden;
        hidden.nonDisplay = true;
        const QString secret = QStringLiteral("SECRET");
        for (int i = 0; i < secret.size(); ++i)
            buf->writeChar(0, 20 + i, core::EBCDIC::charToEBCDIC(secret[i]), hidden);
        QTRY_COMPARE_WITH_TIMEOUT(history->count(), 1, 2000);

        const auto &snap = history->at(0);
        const QString text = QString::fromUtf8(snap.cellData);
        QVERIFY(text.contains(QStringLiteral("Password")));
        QVERIFY(!text.contains(QStringLiteral("SECRET")));
        QVERIFY(history->search(QStringLiteral("SECRET")).isEmpty());

        // The stored cells keep the attribute but not the hidden character
        const auto *raw = reinterpret_cast<const ScreenCell *>(snap.rawCells.constData());
        for (int i = 0; i < secret.size(); ++i) {
            const ScreenCell &cell = raw[20 + i];
            QVERIFY(cell.attributes.nonDisplay);
            QCOMPARE(int(cell.character), 0x40);
        }
    }
};

QTEST_MAIN(TestHistoryNonDisplay)
#include "test_history_nondisplay.moc"
