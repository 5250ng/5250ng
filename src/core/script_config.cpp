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

#include "script_config.h"
#include <QSettings>
#include <algorithm>

namespace core {

namespace {

// Out-of-range stored values fall back to the default
int readMs(const QSettings &settings, const char *key, int min, int max, int fallback) {
    bool ok = false;
    const int value = settings.value(key, fallback).toInt(&ok);
    return (ok && value >= min && value <= max) ? value : fallback;
}

} // namespace

ScriptConfig::ScriptConfig() { load(); }

ScriptConfig &ScriptConfig::instance() {
    static ScriptConfig s;
    return s;
}

void ScriptConfig::setJitterRange(int minMs, int maxMs) {
    m_jitterMinMs = std::min(minMs, maxMs);
    m_jitterMaxMs = std::max(minMs, maxMs);
}

void ScriptConfig::load() {
    QSettings settings;
    settings.beginGroup("Scripts");
    m_expectTimeoutMs = readMs(settings, "expectTimeoutMs", kMinExpectTimeoutMs, kMaxExpectTimeoutMs, 30000);
    m_actionDelayMs = readMs(settings, "actionDelayMs", 0, kMaxDelayMs, 0);
    setJitterRange(readMs(settings, "jitterMinMs", 0, kMaxDelayMs, 0),
                   readMs(settings, "jitterMaxMs", 0, kMaxDelayMs, 0));
    m_confirmDelete = settings.value("confirmDelete", true).toBool();
    settings.endGroup();
}

void ScriptConfig::save() {
    QSettings settings;
    settings.beginGroup("Scripts");
    settings.setValue("expectTimeoutMs", m_expectTimeoutMs);
    settings.setValue("actionDelayMs", m_actionDelayMs);
    settings.setValue("jitterMinMs", m_jitterMinMs);
    settings.setValue("jitterMaxMs", m_jitterMaxMs);
    settings.setValue("confirmDelete", m_confirmDelete);
    settings.endGroup();
}

core::scripting::ExecutionDefaults ScriptConfig::executionDefaults() const {
    core::scripting::ExecutionDefaults d;
    d.expectTimeoutMs = m_expectTimeoutMs;
    d.actionDelayMs = m_actionDelayMs;
    d.jitterMinMs = m_jitterMinMs;
    d.jitterMaxMs = m_jitterMaxMs;
    return d;
}

} // namespace core
