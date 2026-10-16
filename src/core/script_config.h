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
#pragma once

#include <5250script/script_executor.h>

namespace core {

// Settings › Scripts: execution defaults applied to every script run and
// the delete confirmation. Stored in QSettings under "Scripts".
class ScriptConfig {
  public:
    static ScriptConfig &instance();

    void load();
    void save();

    // Allowed ranges (match the Settings page spin boxes)
    static constexpr int kMinExpectTimeoutMs = 1000;
    static constexpr int kMaxExpectTimeoutMs = 120000;
    static constexpr int kMaxDelayMs = 5000;

    int expectTimeoutMs() const { return m_expectTimeoutMs; }
    void setExpectTimeoutMs(int ms) { m_expectTimeoutMs = ms; }

    int actionDelayMs() const { return m_actionDelayMs; }
    void setActionDelayMs(int ms) { m_actionDelayMs = ms; }

    int jitterMinMs() const { return m_jitterMinMs; }
    int jitterMaxMs() const { return m_jitterMaxMs; }
    // A range given as max < min is swapped
    void setJitterRange(int minMs, int maxMs);

    bool confirmDelete() const { return m_confirmDelete; }
    void setConfirmDelete(bool confirm) { m_confirmDelete = confirm; }

    // Starting values for ScriptExecutor::setDefaults()
    core::scripting::ExecutionDefaults executionDefaults() const;

  private:
    ScriptConfig();

    int m_expectTimeoutMs = 30000;
    int m_actionDelayMs = 0;
    int m_jitterMinMs = 0;
    int m_jitterMaxMs = 0;
    bool m_confirmDelete = true;
};

} // namespace core
