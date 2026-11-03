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

#include "core/codepage.h"
#include <QtTest/QtTest>

using core::CodePage;

// NEW-ENVIRON announces KBDTYPE and CHARSET with the session's code page;
// they must belong to that code page, or the host builds the device with a
// US keyboard and an invalid CCSID pair (CHARSET 697 with CODEPAGE 870).
class TestCodepageNewEnviron : public QObject {
    Q_OBJECT

  private slots:
    void pairsForEachCodePage_data() {
        QTest::addColumn<int>("codePage");
        QTest::addColumn<QString>("keyboard");
        QTest::addColumn<int>("charset");
        QTest::newRow("037") << 37 << "USB" << 697;
        QTest::newRow("273") << 273 << "AGB" << 697;
        QTest::newRow("277") << 277 << "DMB" << 697;
        QTest::newRow("278") << 278 << "SWB" << 697;
        QTest::newRow("280") << 280 << "ITB" << 697;
        QTest::newRow("284") << 284 << "SPB" << 697;
        QTest::newRow("285") << 285 << "UKB" << 697;
        QTest::newRow("297") << 297 << "FAB" << 697;
        QTest::newRow("500") << 500 << "INB" << 697;
        QTest::newRow("870") << 870 << "ROB" << 959;
        QTest::newRow("420") << 420 << "CLB" << 235;
        QTest::newRow("424") << 424 << "NCB" << 941;
        QTest::newRow("838") << 838 << "THB" << 1176;
    }

    void pairsForEachCodePage() {
        QFETCH(int, codePage);
        QFETCH(QString, keyboard);
        QFETCH(int, charset);
        const auto id = static_cast<CodePage::ID>(codePage);
        QCOMPARE(CodePage::keyboardType(id), keyboard);
        QCOMPARE(CodePage::characterSet(id), charset);
    }

    // Every supported code page has its own entry (none falls back to the
    // US defaults by omission).
    void everySupportedCodePageIsCovered() {
        int nonUs = 0;
        for (CodePage::ID id : CodePage::supportedCodePages()) {
            QCOMPARE(CodePage::keyboardType(id).size(), 3);
            if (id != CodePage::ID::CP037)
                nonUs += CodePage::keyboardType(id) != QLatin1String("USB");
        }
        QCOMPARE(nonUs, CodePage::supportedCodePages().size() - 1);
    }
};

QTEST_GUILESS_MAIN(TestCodepageNewEnviron)
#include "test_codepage_new_environ.moc"
