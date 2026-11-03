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

#include "core/macro_config.h"
#include "core/script_config.h"
#include "ui/dialogs/settings_dialog.h"
#include "ui/themes/app_theme_setting.h"
#include "ui/themes/manager.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest/QtTest>

// Settings > Save must keep the edits made on every page the user opened,
// not only the page on screen, and the application theme must be stored.
class TestSettingsSaveAllPages : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
        ui::themes::ThemeManager::instance().loadBuiltinThemes();
    }

    void editsOnEveryVisitedPageAreSaved() {
        core::MacroConfig::instance().load();
        const bool timings = core::MacroConfig::instance().recordTimings();

        SettingsDialog dialog;
        // Recording page: flip "record timings"
        QVERIFY(select(dialog, "Scripts", "Recording"));
        QCheckBox *timingsCheck = currentPage(dialog)->findChildren<QCheckBox *>().value(0);
        QVERIFY(timingsCheck);
        timingsCheck->setChecked(!timings);

        // Scripts page: change the EXPECT timeout
        QVERIFY(select(dialog, "Scripts", "General"));
        QSpinBox *timeout = currentPage(dialog)->findChildren<QSpinBox *>().value(0);
        QVERIFY(timeout);
        timeout->setValue(12345);

        // Looking at the 5250 Theme page must not re-theme the sessions
        QSignalSpy applied(findTerminalThemePage(dialog), SIGNAL(applyToAllRequested(ui::themes::TerminalTheme)));
        QVERIFY(select(dialog, "Themes", "5250 Theme"));

        // Application theme page: pick a theme other than the current one
        QVERIFY(select(dialog, "Themes", "Application Theme"));
        QComboBox *themes = currentPage(dialog)->findChildren<QComboBox *>().value(0);
        QVERIFY(themes && themes->count() > 1);
        themes->setCurrentIndex(themes->currentIndex() == 0 ? 1 : 0);
        const QString theme = themes->currentData().toString();

        // Save while the theme page is on screen; dismiss the confirmation
        QTimer dismiss;
        connect(&dismiss, &QTimer::timeout, this, []() {
            if (QWidget *modal = QApplication::activeModalWidget())
                for (QPushButton *b : modal->findChildren<QPushButton *>())
                    if (b->text() == "OK") { b->click(); return; }
        });
        dismiss.start(20);
        QPushButton *save = nullptr;
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text() == "Save") save = b;
        QVERIFY(save);
        save->click();
        dismiss.stop();

        core::MacroConfig::instance().load();
        core::ScriptConfig::instance().load();
        QCOMPARE(core::MacroConfig::instance().recordTimings(), !timings);
        QCOMPARE(core::ScriptConfig::instance().expectTimeoutMs(), 12345);
        QCOMPARE(ui::themes::savedAppTheme(), theme);
        QCOMPARE(applied.size(), 0);
    }

  private:
    static bool select(SettingsDialog &dialog, const QString &parent, const QString &child) {
        auto *tree = dialog.findChild<QTreeWidget *>();
        for (QTreeWidgetItem *item : tree->findItems(child, Qt::MatchExactly | Qt::MatchRecursive)) {
            if (item->parent() && item->parent()->text(0) == parent) {
                tree->setCurrentItem(item);
                return true;
            }
        }
        return false;
    }
    static QObject *findTerminalThemePage(SettingsDialog &dialog) {
        for (QObject *o : dialog.findChildren<QObject *>())
            if (o->metaObject()->indexOfSignal("applyToAllRequested(ui::themes::TerminalTheme)") >= 0)
                return o;
        return nullptr;
    }

    // The settings pages are the dialog's first stacked widget
    static QWidget *currentPage(SettingsDialog &dialog) {
        auto *stack = dialog.findChild<QStackedWidget *>();
        return stack ? stack->currentWidget() : nullptr;
    }

    QTemporaryDir m_dir;
};

QTEST_MAIN(TestSettingsSaveAllPages)
#include "test_settings_save_all_pages.moc"
