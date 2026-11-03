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

#include "ui/themes/terminal_theme_manager.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using ui::themes::TerminalTheme;
using ui::themes::TerminalThemeManager;

// Regression tests for issue #287: applying a built-in theme edited in the
// theme editor registered it under the built-in's id as a user theme, so the
// built-in was replaced and could then be deleted, leaving new tabs without
// their default theme.
class TestEditedBuiltinTheme : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void testEditedBuiltinBecomesDerivedTheme();
    void testUnchangedBuiltinKeepsItsId();
    void testEditedUserThemeKeepsIdentity();
    void testExportDoesNotRegister();

  private:
    // A theme as the editor builds it from its widgets: no identity
    static TerminalTheme fromEditor(const QString &id);
    QTemporaryDir m_dir;
};

void TestEditedBuiltinTheme::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    TerminalThemeManager::instance().loadBuiltinThemes();
    QVERIFY(TerminalThemeManager::instance().theme(QStringLiteral("classic_green")).builtin);
}

TerminalTheme TestEditedBuiltinTheme::fromEditor(const QString &id) {
    TerminalTheme t = TerminalThemeManager::instance().resolvedTheme(id);
    t.builtin = false;
    t.description.clear();
    t.parentThemeId.clear();
    return t;
}

void TestEditedBuiltinTheme::testEditedBuiltinBecomesDerivedTheme() {
    auto &mgr = TerminalThemeManager::instance();
    const QString builtinId = QStringLiteral("classic_green");
    const QColor original = mgr.resolvedTheme(builtinId).colorGreen;

    TerminalTheme edited = fromEditor(builtinId);
    edited.colorGreen = QColor(1, 2, 3);
    const QString id = mgr.applyEditedTheme(edited, true);

    QVERIFY(id != builtinId);
    QCOMPARE(mgr.resolvedTheme(id).colorGreen, QColor(1, 2, 3));
    QCOMPARE(mgr.theme(id).parentThemeId, builtinId);
    QVERIFY(!mgr.theme(id).builtin);
    // The built-in itself is unchanged and still cannot be deleted
    QVERIFY(mgr.theme(builtinId).builtin);
    QCOMPARE(mgr.resolvedTheme(builtinId).colorGreen, original);
    QVERIFY(!mgr.deleteUserTheme(builtinId));
    QVERIFY(mgr.hasTheme(builtinId));
    QVERIFY(mgr.deleteUserTheme(id));
}

void TestEditedBuiltinTheme::testUnchangedBuiltinKeepsItsId() {
    auto &mgr = TerminalThemeManager::instance();
    const int before = mgr.availableThemes().size();
    QCOMPARE(mgr.applyEditedTheme(fromEditor(QStringLiteral("classic_green")), false),
             QStringLiteral("classic_green"));
    QCOMPARE(mgr.availableThemes().size(), before);
    QVERIFY(mgr.theme(QStringLiteral("classic_green")).builtin);
}

void TestEditedBuiltinTheme::testEditedUserThemeKeepsIdentity() {
    auto &mgr = TerminalThemeManager::instance();
    TerminalTheme user = mgr.duplicateTheme(QStringLiteral("classic_green"), QStringLiteral("Mine"));
    user.description = QStringLiteral("my theme");
    QVERIFY(mgr.saveUserTheme(user));

    TerminalTheme edited = fromEditor(user.id);
    edited.colorRed = QColor(9, 9, 9);
    QCOMPARE(mgr.applyEditedTheme(edited, true), user.id);
    QCOMPARE(mgr.theme(user.id).description, QStringLiteral("my theme"));
    QCOMPARE(mgr.resolvedTheme(user.id).colorRed, QColor(9, 9, 9));
    QVERIFY(mgr.deleteUserTheme(user.id));
}

void TestEditedBuiltinTheme::testExportDoesNotRegister() {
    auto &mgr = TerminalThemeManager::instance();
    TerminalTheme edited = fromEditor(QStringLiteral("classic_green"));
    edited.colorGreen = QColor(4, 5, 6);
    QVERIFY(m_dir.isValid());
    QVERIFY(mgr.exportTheme(edited, m_dir.filePath(QStringLiteral("x.5250theme"))));
    QVERIFY(mgr.theme(QStringLiteral("classic_green")).builtin);
    QVERIFY(mgr.resolvedTheme(QStringLiteral("classic_green")).colorGreen != QColor(4, 5, 6));
}

QTEST_MAIN(TestEditedBuiltinTheme)
#include "test_edited_builtin_theme.moc"
