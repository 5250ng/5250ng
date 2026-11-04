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

#include "core/keyboard_mapping.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QSignalSpy>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #280: window shortcuts (F11 Full Screen, ...)
// took keys bound to 5250 actions before the focused screen saw them. Qt
// gives the focus widget a ShortcutOverride event first; the screen must
// accept it exactly for the chords it would act on.
class TestShortcutOverride : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testBoundKeyOverridesShortcut();
    void testUnboundChordLeavesShortcut();
    void testReadOnlyScreenLeavesShortcut();
    void testLockedKeyboardLeavesShortcut();
    void testModifiedFKeyFallbackLeavesShortcut();
    void testBoundKeyReachesSessionInWindow();

  private:
    // Whether the screen claims the chord ahead of window shortcuts
    bool overrides(int key, Qt::KeyboardModifiers mods = Qt::NoModifier);

    Q5250ScreenWidget *m_screen = nullptr;
};

void TestShortcutOverride::init() {
    core::KeyboardMapping::instance().resetToDefaults();
    m_screen = new Q5250ScreenWidget();
    m_screen->setScreenSize(24, 80);
    m_screen->setKeyboardState(KeyboardState::Unlocked);
}

void TestShortcutOverride::cleanup() {
    delete m_screen;
    m_screen = nullptr;
}

bool TestShortcutOverride::overrides(int key, Qt::KeyboardModifiers mods) {
    QKeyEvent ev(QEvent::ShortcutOverride, key, mods);
    ev.ignore();
    QApplication::sendEvent(m_screen, &ev);
    return ev.isAccepted();
}

void TestShortcutOverride::testBoundKeyOverridesShortcut() {
    // F11 is PF11 by default and Full Screen's shortcut on Linux/Windows
    QVERIFY(overrides(Qt::Key_F11));
}

void TestShortcutOverride::testUnboundChordLeavesShortcut() {
    // Ctrl+N is not bound to a 5250 action: New Session keeps it
    QVERIFY(!overrides(Qt::Key_N, Qt::ControlModifier));
}

void TestShortcutOverride::testReadOnlyScreenLeavesShortcut() {
    // A read-only (MCP or replay) screen ignores the key, so the window
    // shortcut keeps working there
    m_screen->setReadOnly(true);
    QVERIFY(!overrides(Qt::Key_F11));
}

void TestShortcutOverride::testLockedKeyboardLeavesShortcut() {
    // While the host has the keyboard locked the screen would drop F11, so
    // Full Screen keeps working; Attn still goes to the session
    m_screen->setKeyboardState(KeyboardState::Locked);
    QVERIFY(!overrides(Qt::Key_F11));
    QVERIFY(overrides(Qt::Key_Escape, Qt::ControlModifier));
}

void TestShortcutOverride::testModifiedFKeyFallbackLeavesShortcut() {
    // Ctrl+F6 has no binding of its own (it only follows F6's), so the
    // window's next-tab shortcut keeps it
    QVERIFY(!overrides(Qt::Key_F6, Qt::ControlModifier));
}

void TestShortcutOverride::testBoundKeyReachesSessionInWindow() {
    // End to end through Qt's shortcut map: needs an active window, which
    // a platform without a window manager (xvfb) may not provide
    QMainWindow window;
    m_screen->setParent(nullptr);
    window.setCentralWidget(m_screen);
    auto *fullScreen = new QAction(&window);
    fullScreen->setShortcut(QKeySequence(Qt::Key_F11));
    window.addAction(fullScreen);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    if (!QTest::qWaitForWindowActive(&window, 2000))
        QSKIP("no active window on this platform");
    m_screen->setFocus();
    QSignalSpy triggered(fullScreen, &QAction::triggered);
    QSignalSpy sent(m_screen, &Q5250ScreenWidget::inputReady);
    QTest::keyClick(m_screen, Qt::Key_F11);
    QCOMPARE(triggered.count(), 0);
    QCOMPARE(sent.count(), 1);
    QCOMPARE(static_cast<uint8_t>(sent.at(0).at(0).toByteArray().at(2)), uint8_t(0x3B)); // PF11 AID
    window.takeCentralWidget(); // m_screen is deleted in cleanup()
}

QTEST_MAIN(TestShortcutOverride)
#include "test_shortcut_override.moc"
