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
#include <QLabel>
#include <QPushButton>
#include <QtTest/QtTest>

// Regression test for issue #355: the Settings 5250 Theme page's Save
// applies its theme to the open sessions only, without saying so, and the
// page offers no Apply-to-current control. The embedded page says what
// Save does.
class TestThemePageNote : public QObject {
    Q_OBJECT

  private slots:
    void embeddedPageSaysWhatSaveDoes() {
        SessionSettingsDialog page;
        page.setEmbeddedMode(true);
        page.show();
        QLabel *note = nullptr;
        for (QLabel *l : page.findChildren<QLabel *>())
            if (l->text().contains(QStringLiteral("open now only")))
                note = l;
        QVERIFY(note);
        QVERIFY(note->isVisible());
        for (QPushButton *b : page.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Apply"))
                QVERIFY(!b->isVisible());
    }

    void standaloneDialogHasNoNote() {
        SessionSettingsDialog dialog;
        dialog.show();
        for (QLabel *l : dialog.findChildren<QLabel *>())
            if (l->text().contains(QStringLiteral("open now only")))
                QVERIFY(!l->isVisible());
    }
};

QTEST_MAIN(TestThemePageNote)
#include "test_theme_page_note.moc"
