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
#include <QApplication>
#include <QClipboard>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #359: View > History Back/Forward changed the
// history index and the footer only; the widget kept painting the live
// screen.
class TestHistoryView : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testHistoryViewPaintsSnapshot();
    void testExitPaintsLiveScreenAgain();
    void testKeyReturnsToLiveScreen();
    void testNewSnapshotKeepsViewedScreen();
    void testModifierKeyStaysInHistory();
    void testCopyTakesViewedScreen();
    void testInputRefusedWhileViewing();
    void testClickDoesNotMoveLiveCursor();
    void testDroppedSnapshotReturnsToLiveScreen();

  private:
    void drawScreen(const QString &text);
    QImage paint();

    Q5250ScreenWidget *m_widget = nullptr;
};

void TestHistoryView::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_widget->resize(640, 384);
    m_widget->setKeyboardState(KeyboardState::Unlocked);
    m_widget->screenHistory()->clear();
}

void TestHistoryView::cleanup() {
    delete m_widget;
    m_widget = nullptr;
}

// Writes the text cell by cell, as the host data stream does, and waits for
// the screen to be recorded in history
void TestHistoryView::drawScreen(const QString &text) {
    auto *buf = m_widget->screenBuffer();
    const int before = m_widget->screenHistory()->count();
    buf->clear();
    buf->setCursorVisible(false);
    for (int i = 0; i < text.size(); ++i)
        buf->writeChar(0, i, core::EBCDIC::charToEBCDIC(text[i]));
    QTRY_COMPARE_WITH_TIMEOUT(m_widget->screenHistory()->count(), before + 1, 2000);
}

QImage TestHistoryView::paint() {
    return m_widget->grab().toImage();
}

void TestHistoryView::testHistoryViewPaintsSnapshot() {
    drawScreen(QStringLiteral("FIRST SCREEN"));
    const QImage first = paint();
    drawScreen(QStringLiteral("SECOND SCREEN"));
    const QImage second = paint();
    QVERIFY(first != second);

    m_widget->viewHistoryScreen(1); // index 0 is the newest snapshot
    QCOMPARE(paint(), first);
}

void TestHistoryView::testExitPaintsLiveScreenAgain() {
    drawScreen(QStringLiteral("FIRST SCREEN"));
    drawScreen(QStringLiteral("SECOND SCREEN"));
    const QImage live = paint();
    m_widget->viewHistoryScreen(1);
    QVERIFY(paint() != live);
    m_widget->exitHistoryView();
    QCOMPARE(paint(), live);
}

void TestHistoryView::testKeyReturnsToLiveScreen() {
    m_widget->screenBuffer()->setField(5, 10, 5, false);
    drawScreen(QStringLiteral("FIRST SCREEN"));
    m_widget->screenBuffer()->setField(5, 10, 5, false);
    m_widget->moveCursor(5, 10);
    m_widget->viewHistoryScreen(0);
    QTest::keyClick(m_widget, Qt::Key_A);
    QVERIFY(!m_widget->isViewingHistory());
    // The key only left history view; it typed nothing
    QCOMPARE(int(m_widget->screenBuffer()->character(5, 10)), 0x40);
}

void TestHistoryView::testNewSnapshotKeepsViewedScreen() {
    drawScreen(QStringLiteral("FIRST SCREEN"));
    const QImage first = paint();
    drawScreen(QStringLiteral("SECOND SCREEN"));
    m_widget->viewHistoryScreen(1);
    // The host draws another screen while the user looks at history
    drawScreen(QStringLiteral("THIRD SCREEN"));
    QCOMPARE(m_widget->historyIndex(), 2);
    QCOMPARE(paint(), first);
}

void TestHistoryView::testModifierKeyStaysInHistory() {
    drawScreen(QStringLiteral("FIRST SCREEN"));
    m_widget->viewHistoryScreen(0);
    QTest::keyPress(m_widget, Qt::Key_Control);
    QTest::keyRelease(m_widget, Qt::Key_Control);
    QTest::keyPress(m_widget, Qt::Key_Alt);
    QTest::keyRelease(m_widget, Qt::Key_Alt);
    QVERIFY(m_widget->isViewingHistory());
}

void TestHistoryView::testCopyTakesViewedScreen() {
    drawScreen(QStringLiteral("FIRST SCREEN"));
    drawScreen(QStringLiteral("SECOND SCREEN"));
    m_widget->viewHistoryScreen(1);
    m_widget->selectAll();
    QTest::keyClick(m_widget, Qt::Key_C, Qt::ControlModifier);
    QVERIFY(m_widget->isViewingHistory());
    const QString copied = QApplication::clipboard()->text();
    QVERIFY2(copied.startsWith(QStringLiteral("FIRST SCREEN")), qPrintable(copied.left(20)));
}

void TestHistoryView::testInputRefusedWhileViewing() {
    // Hotspots, the virtual keyboard and paste ask acceptsInput() first
    drawScreen(QStringLiteral("FIRST SCREEN"));
    QVERIFY(m_widget->acceptsInput(core::MappedAction::Enter));
    m_widget->viewHistoryScreen(0);
    QVERIFY(!m_widget->acceptsInput(core::MappedAction::Enter));
    QVERIFY(!m_widget->acceptsInput(core::MappedAction::None));
}

void TestHistoryView::testClickDoesNotMoveLiveCursor() {
    drawScreen(QStringLiteral("FIRST SCREEN"));
    m_widget->moveCursor(0, 0);
    m_widget->viewHistoryScreen(0);
    m_widget->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_widget));
    QTest::mouseClick(m_widget, Qt::LeftButton, Qt::NoModifier, m_widget->rect().center());
    QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(0, 0));
}

void TestHistoryView::testDroppedSnapshotReturnsToLiveScreen() {
    m_widget->screenHistory()->setMaxDepth(2);
    drawScreen(QStringLiteral("FIRST SCREEN"));
    drawScreen(QStringLiteral("SECOND SCREEN"));
    m_widget->viewHistoryScreen(1); // the oldest, dropped by the next push
    auto *buf = m_widget->screenBuffer();
    buf->clear();
    const QString third = QStringLiteral("THIRD SCREEN");
    for (int i = 0; i < third.size(); ++i)
        buf->writeChar(0, i, core::EBCDIC::charToEBCDIC(third[i]));
    QTRY_VERIFY_WITH_TIMEOUT(!m_widget->isViewingHistory(), 2000);
}

QTEST_MAIN(TestHistoryView)
#include "test_history_view.moc"
