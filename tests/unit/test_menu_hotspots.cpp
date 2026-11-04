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
#include "core/hotspot_detector.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using core::Hotspot;
using core::HotspotDetector;
using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #295: menu hotspots matched one option per
// line (so the second column of a two-column menu selected the first), and
// a click typed the option at wherever the cursor was.
class TestMenuHotspots : public QObject {
    Q_OBJECT

  private slots:
    void testTwoColumnMenuHasOneHotspotPerOption();
    void testIbmiMenuWithCommandColumn();
    void testOptionGoesIntoSelectionField();
    void testTextIsNotAMenu_data();
    void testTextIsNotAMenu();
    void testThreeColumnMenu();
    void testSingleCharacterLabels();
    void testOptionReplacesWholeField();
    void testIcOutsideFieldFallsBackToCursorField();
    void testNoInputFieldDoesNothing();
    void testReadOnlyDoesNothing();

  private:
    static QVector<Hotspot> menuItems(const QStringList &lines, int cols = 80);
};

QVector<Hotspot> TestMenuHotspots::menuItems(const QStringList &lines, int cols) {
    QVector<QChar> text;
    for (const QString &l : lines) {
        const QString padded = l.leftJustified(cols, ' ', true);
        for (QChar c : padded) text.append(c);
    }
    HotspotDetector detector;
    detector.setEnabled(true);
    QVector<Hotspot> items;
    for (const Hotspot &h : detector.detect(text, lines.size(), cols))
        if (h.type == Hotspot::MenuItem) items.append(h);
    return items;
}

void TestMenuHotspots::testTwoColumnMenuHasOneHotspotPerOption() {
    const auto items = menuItems({QStringLiteral("      1. Foo tasks                     11. Bar tasks")});
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].menuNumber, QStringLiteral("1"));
    QCOMPARE(items[1].menuNumber, QStringLiteral("11"));
    // Clicking "Bar" selects 11, not 1
    const int barCol = QStringLiteral("      1. Foo tasks                     11. Bar tasks").indexOf("Bar");
    const Hotspot *hit = HotspotDetector::hotspotAt(items, 0, barCol);
    QVERIFY(hit);
    QCOMPARE(hit->menuNumber, QStringLiteral("11"));
    // The gap between the columns belongs to neither option
    QVERIFY(!HotspotDetector::hotspotAt(items, 0, barCol - 10));
}

void TestMenuHotspots::testIbmiMenuWithCommandColumn() {
    // GO MAJOR on IBM i 7.5: the command name column is not an option
    const QStringList screen = {
        QStringLiteral(" MAJOR                        Major Command Groups"),
        QStringLiteral(" Select one of the following:"),
        QStringLiteral("      1. Select Command by Name                                      SLTCMD"),
        QStringLiteral("     10. Print Commands                                              CMDPRT"),
        QStringLiteral(" Selection or command"),
        QStringLiteral(" ===>"),
    };
    const auto items = menuItems(screen);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].menuNumber, QStringLiteral("1"));
    QCOMPARE(items[0].label, QStringLiteral("1. Select Command by Name"));
    QCOMPARE(items[1].menuNumber, QStringLiteral("10"));
    QCOMPARE(items[1].label, QStringLiteral("10. Print Commands"));
}

void TestMenuHotspots::testOptionGoesIntoSelectionField() {
    Q5250ScreenWidget widget;
    widget.setScreenSize(24, 80);
    widget.setKeyboardState(KeyboardState::Unlocked);
    auto *buf = widget.screenBuffer();
    buf->setField(5, 10, 20, false); // some other input field
    buf->setField(19, 7, 10, false); // the "===>" selection field
    // Left over from an earlier attempt
    buf->writeChar(19, 7, core::EBCDIC::charToEBCDIC('9'));
    buf->writeChar(19, 8, core::EBCDIC::charToEBCDIC('9'));
    widget.setICAddress(19, 7); // where the host put the cursor
    widget.moveCursor(5, 10);   // the user moved it since

    QSignalSpy sent(&widget, &Q5250ScreenWidget::inputReady);
    widget.activateMenuOption(QStringLiteral("11"));
    QCOMPARE(int(buf->character(19, 7)), int(core::EBCDIC::charToEBCDIC('1')));
    QCOMPARE(int(buf->character(19, 8)), int(core::EBCDIC::charToEBCDIC('1')));
    QVERIFY(buf->character(5, 10) == 0x00 || buf->character(5, 10) == 0x40);
    QCOMPARE(sent.count(), 1); // Enter
    QCOMPARE(static_cast<uint8_t>(sent.at(0).at(0).toByteArray().at(2)), uint8_t(0xF1));
}

void TestMenuHotspots::testTextIsNotAMenu_data() {
    // "1." in the middle of ordinary text is not a menu option
    QTest::addColumn<QString>("line");
    QTest::newRow("sentence") << "  Press Enter to continue.  1. Not a menu item?";
    QTest::newRow("after F-key") << "  F3=Exit  1. Weird mix";
    QTest::newRow("words first") << "  Foo bar  1. Second option";
    QTest::newRow("step") << "  Step  1.  Install the product";
    QTest::newRow("line of") << "  Line  2.  of 5";
    QTest::newRow("page") << "  Page  1. of  3";
    QTest::newRow("total") << "  Total:  12. 34";
    QTest::newRow("time") << "  Time . . :   12. 30. 15";
    QTest::newRow("version") << "  Version  7. 5 installed";
    QTest::newRow("options list") << "  2=Change  4=Delete  5=Display";
    QTest::newRow("size") << "     1.5 GB";
    QTest::newRow("empty label") << "  3. ";
}

void TestMenuHotspots::testTextIsNotAMenu() {
    QFETCH(QString, line);
    const auto items = menuItems({line});
    QVERIFY2(items.isEmpty(), qPrintable(items.isEmpty() ? QString() : items.first().label));
}

void TestMenuHotspots::testSingleCharacterLabels() {
    // A one-character label must not run on into the next column
    auto items = menuItems({QStringLiteral("      1. X                     11. Bar tasks")});
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].label, QStringLiteral("1. X"));
    QCOMPARE(items[1].menuNumber, QStringLiteral("11"));
    items = menuItems({QStringLiteral("  1. A  2. B  3. C  4. D")});
    QCOMPARE(items.size(), 4);
    QCOMPARE(items[3].label, QStringLiteral("4. D"));
}

void TestMenuHotspots::testThreeColumnMenu() {
    const auto items = menuItems({QStringLiteral("  1. One        2. Two        3. Three")});
    QCOMPARE(items.size(), 3);
    QCOMPARE(items[2].menuNumber, QStringLiteral("3"));
    QCOMPARE(items[2].label, QStringLiteral("3. Three"));
}

void TestMenuHotspots::testOptionReplacesWholeField() {
    // No IC address and the cursor after an earlier "99": the option still
    // replaces the whole field, not only what follows the cursor
    Q5250ScreenWidget widget;
    widget.setScreenSize(24, 80);
    widget.setKeyboardState(KeyboardState::Unlocked);
    auto *buf = widget.screenBuffer();
    buf->setField(19, 7, 10, false);
    buf->writeChar(19, 7, core::EBCDIC::charToEBCDIC('9'));
    buf->writeChar(19, 8, core::EBCDIC::charToEBCDIC('9'));
    widget.moveCursor(19, 9);
    widget.activateMenuOption(QStringLiteral("11"));
    QCOMPARE(int(buf->character(19, 7)), int(core::EBCDIC::charToEBCDIC('1')));
    QCOMPARE(int(buf->character(19, 8)), int(core::EBCDIC::charToEBCDIC('1')));
    QVERIFY(buf->character(19, 9) == 0x00 || buf->character(19, 9) == 0x40);
}

void TestMenuHotspots::testIcOutsideFieldFallsBackToCursorField() {
    Q5250ScreenWidget widget;
    widget.setScreenSize(24, 80);
    widget.setKeyboardState(KeyboardState::Unlocked);
    auto *buf = widget.screenBuffer();
    buf->setField(19, 7, 10, false);
    buf->writeChar(3, 3, core::EBCDIC::charToEBCDIC('M')); // screen text
    widget.setICAddress(3, 3); // stale or odd IC outside every field
    widget.moveCursor(19, 7);
    QSignalSpy sent(&widget, &Q5250ScreenWidget::inputReady);
    widget.activateMenuOption(QStringLiteral("5"));
    QCOMPARE(int(buf->character(3, 3)), int(core::EBCDIC::charToEBCDIC('M')));
    QCOMPARE(int(buf->character(19, 7)), int(core::EBCDIC::charToEBCDIC('5')));
    QCOMPARE(sent.count(), 1);
}

void TestMenuHotspots::testNoInputFieldDoesNothing() {
    Q5250ScreenWidget widget;
    widget.setScreenSize(24, 80);
    widget.setKeyboardState(KeyboardState::Unlocked);
    widget.moveCursor(3, 3);
    QSignalSpy sent(&widget, &Q5250ScreenWidget::inputReady);
    widget.activateMenuOption(QStringLiteral("5"));
    QCOMPARE(sent.count(), 0);
    QCOMPARE(widget.screenBuffer()->cursorPosition(), QPoint(3, 3));
}

void TestMenuHotspots::testReadOnlyDoesNothing() {
    Q5250ScreenWidget widget;
    widget.setScreenSize(24, 80);
    widget.setKeyboardState(KeyboardState::Unlocked);
    widget.screenBuffer()->setField(19, 7, 10, false);
    widget.moveCursor(19, 7);
    widget.setReadOnly(true);
    QSignalSpy sent(&widget, &Q5250ScreenWidget::inputReady);
    widget.activateMenuOption(QStringLiteral("5"));
    QCOMPARE(sent.count(), 0);
}

QTEST_MAIN(TestMenuHotspots)
#include "test_menu_hotspots.moc"
