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

#include "ui/dialogs/session_settings_dialog.h"
#include "ui/dialogs/theme_preview_widget.h"
#include <QLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QtTest/QtTest>

// Regression test for issue #365: the Session Settings preview was the last
// item of a long scroll area, so it was out of view while the controls at
// the top were being changed, and those controls were out of view while it
// was shown.
class TestSettingsPreviewVisible : public QObject {
    Q_OBJECT

    static void checkPreviewStaysVisible(SessionSettingsDialog &dialog) {
        auto *preview = dialog.findChild<ThemePreviewWidget *>();
        auto *scroll = dialog.findChild<QScrollArea *>();
        QVERIFY(preview);
        QVERIFY(scroll);
        QScrollBar *bar = scroll->verticalScrollBar();
        for (int value : {bar->minimum(), bar->maximum()}) {
            bar->setValue(value);
            QCoreApplication::processEvents();
            QVERIFY2(!preview->visibleRegion().isEmpty(),
                     qPrintable(QString("preview hidden at scroll value %1").arg(value)));
        }
        QVERIFY2(!scroll->isAncestorOf(preview), "preview scrolls with the settings");
        // The first control of the settings is visible at the top with it
        bar->setValue(bar->minimum());
        QCoreApplication::processEvents();
        QWidget *content = scroll->widget();
        QVERIFY(content);
        QWidget *first = content->layout()->itemAt(0)->widget();
        QVERIFY(first);
        QVERIFY(!first->visibleRegion().isEmpty());
    }

  private slots:
    void standaloneDialog() {
        SessionSettingsDialog dialog;
        dialog.resize(620, 780);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        checkPreviewStaysVisible(dialog);
    }

    void embeddedPage() {
        SessionSettingsDialog page;
        page.setEmbeddedMode(true);
        page.resize(600, 560);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        checkPreviewStaysVisible(page);
    }
};

QTEST_MAIN(TestSettingsPreviewVisible)
#include "test_settings_preview_visible.moc"
