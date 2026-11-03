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
#include <QApplication>
#include <QClipboard>
#include <QtTest/QtTest>

using ui::widgets::Q5250ScreenWidget;

// Regression test for issue #382: Copy put the characters of non-display
// cells (a typed password) on the clipboard although the screen hides them.
class TestCopyNonDisplay : public QObject {
    Q_OBJECT

  private slots:
    void copyBlanksNonDisplayCells() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        auto *buf = widget.screenBuffer();
        const QString label = QStringLiteral("Password");
        for (int i = 0; i < label.size(); ++i)
            buf->writeChar(0, i, core::EBCDIC::charToEBCDIC(label[i]));
        ui::widgets::CellAttributes hidden;
        hidden.nonDisplay = true;
        const QString secret = QStringLiteral("SECRET");
        for (int i = 0; i < secret.size(); ++i)
            buf->writeChar(0, 20 + i, core::EBCDIC::charToEBCDIC(secret[i]), hidden);

        widget.selectAll();
        widget.copySelection();
        const QString copied = QApplication::clipboard()->text();
        QVERIFY2(copied.startsWith(QStringLiteral("Password")), qPrintable(copied.left(30)));
        QVERIFY2(!copied.contains(QStringLiteral("SECRET")), qPrintable(copied.left(30)));
    }
};

QTEST_MAIN(TestCopyNonDisplay)
#include "test_copy_nondisplay.moc"
