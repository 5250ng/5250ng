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

#include "ui/window_geometry.h"
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QtTest/QtTest>

// Regression tests for issue #412: the main window's size and position were
// not saved, so every launch started at the default geometry.
class TestWindowGeometry : public QObject {
    Q_OBJECT

    const QString kKey = QStringLiteral("test/geometry");

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("5250ng-test-window-geometry"));
        QCoreApplication::setApplicationName(QStringLiteral("test_window_geometry"));
        QSettings().clear();
    }
    void cleanupTestCase() { QSettings().clear(); }

    void nothingSavedKeepsTheDefault() {
        QWidget window;
        window.resize(900, 645);
        QVERIFY(!ui::restoreWindowGeometry(&window, kKey));
        QCOMPARE(window.size(), QSize(900, 645));
    }

    void savedGeometryIsRestored() {
        {
            QWidget window;
            window.resize(731, 517);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            ui::saveWindowGeometry(&window, kKey);
        }
        QWidget restored;
        restored.resize(900, 645);
        QVERIFY(ui::restoreWindowGeometry(&restored, kKey));
        QCOMPARE(restored.size(), QSize(731, 517));
    }

    void corruptValueIsIgnored() {
        QSettings().setValue(kKey, QByteArray("not a geometry"));
        QWidget window;
        window.resize(900, 645);
        QVERIFY(!ui::restoreWindowGeometry(&window, kKey));
        QCOMPARE(window.size(), QSize(900, 645));
    }

    void fullScreenIsNotSaved() {
        QSettings().remove(kKey);
        QWidget window;
        window.showFullScreen();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        if (!window.isFullScreen())
            QSKIP("platform cannot make the window full screen");
        ui::saveWindowGeometry(&window, kKey);
        QVERIFY(!QSettings().contains(kKey));
    }
};

QTEST_MAIN(TestWindowGeometry)
#include "test_window_geometry.moc"
