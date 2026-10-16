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

#include "core/script_config.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using core::ScriptConfig;

class TestScriptConfig : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void init();
    void testDefaults();
    void testSaveAndLoad();
    void testJitterRangeIsOrdered();
    void testOutOfRangeValuesFallBack();

  private:
    QTemporaryDir m_settingsDir;
};

void TestScriptConfig::initTestCase() {
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void TestScriptConfig::init() {
    QSettings().remove("Scripts");
    ScriptConfig::instance().load();
}

void TestScriptConfig::testDefaults() {
    const auto &cfg = ScriptConfig::instance();
    QCOMPARE(cfg.expectTimeoutMs(), 30000);
    QCOMPARE(cfg.actionDelayMs(), 0);
    QCOMPARE(cfg.jitterMinMs(), 0);
    QCOMPARE(cfg.jitterMaxMs(), 0);
    QCOMPARE(cfg.confirmDelete(), true);
    // Same as the executor's built-in defaults
    const auto d = cfg.executionDefaults();
    const core::scripting::ExecutionDefaults builtIn;
    QCOMPARE(d.expectTimeoutMs, builtIn.expectTimeoutMs);
    QCOMPARE(d.actionDelayMs, builtIn.actionDelayMs);
}

void TestScriptConfig::testSaveAndLoad() {
    auto &cfg = ScriptConfig::instance();
    cfg.setExpectTimeoutMs(60000);
    cfg.setActionDelayMs(250);
    cfg.setJitterRange(100, 400);
    cfg.setConfirmDelete(false);
    cfg.save();

    // Reset in memory, then read back what was stored
    cfg.setExpectTimeoutMs(1);
    cfg.setActionDelayMs(1);
    cfg.setJitterRange(1, 1);
    cfg.setConfirmDelete(true);
    cfg.load();
    QCOMPARE(cfg.expectTimeoutMs(), 60000);
    QCOMPARE(cfg.actionDelayMs(), 250);
    QCOMPARE(cfg.jitterMinMs(), 100);
    QCOMPARE(cfg.jitterMaxMs(), 400);
    QCOMPARE(cfg.confirmDelete(), false);

    const auto d = cfg.executionDefaults();
    QCOMPARE(d.expectTimeoutMs, 60000);
    QCOMPARE(d.actionDelayMs, 250);
    QCOMPARE(d.jitterMinMs, 100);
    QCOMPARE(d.jitterMaxMs, 400);
}

void TestScriptConfig::testJitterRangeIsOrdered() {
    auto &cfg = ScriptConfig::instance();
    cfg.setJitterRange(500, 200);
    QCOMPARE(cfg.jitterMinMs(), 200);
    QCOMPARE(cfg.jitterMaxMs(), 500);
}

void TestScriptConfig::testOutOfRangeValuesFallBack() {
    QSettings settings;
    settings.beginGroup("Scripts");
    settings.setValue("expectTimeoutMs", 5);        // below the 1000 ms minimum
    settings.setValue("actionDelayMs", 999999);     // above the maximum
    settings.setValue("jitterMinMs", "not a number");
    settings.setValue("jitterMaxMs", 300);
    settings.endGroup();
    settings.sync();

    auto &cfg = ScriptConfig::instance();
    cfg.load();
    QCOMPARE(cfg.expectTimeoutMs(), 30000);
    QCOMPARE(cfg.actionDelayMs(), 0);
    QCOMPARE(cfg.jitterMinMs(), 0);
    QCOMPARE(cfg.jitterMaxMs(), 300);
}

QTEST_MAIN(TestScriptConfig)
#include "test_script_config.moc"
