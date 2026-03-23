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

#include "../dialogs/settings_dialog.h"
#include "../main_window.h"
#include "ui/themes/terminal_theme_manager.h"

using namespace core;

/**
 * Open the application Settings dialog.
 *
 * Presents categories in a sidebar and content on the right:
 *   - Application Theme: UI theme selection
 *   - 5250 Theme: full terminal theme editor (colors, fonts, CRT, etc.)
 */
void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);

    // Pre-load the active session's terminal theme into the editor
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        Session *s = m_sessions[m_activeIndex];
        auto &mgr = ui::themes::TerminalThemeManager::instance();
        QString themeId = s->config.terminalThemeId();
        if (themeId.isEmpty() || !mgr.hasTheme(themeId)) {
            themeId = ui::themes::TerminalThemeManager::defaultThemeId();
        }
        dlg.setTerminalTheme(mgr.resolvedTheme(themeId));
    }

    // Apply to current session
    connect(&dlg, &SettingsDialog::terminalThemeApplyRequested,
            this, [this](const ui::themes::TerminalTheme &t) {
        if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
        ui::themes::TerminalThemeManager::instance().registerTheme(t);
        applyThemeToSession(m_sessions[m_activeIndex], t.id);
        m_currentSession.setTerminalThemeId(t.id);
    });

    // Apply to all sessions
    connect(&dlg, &SettingsDialog::terminalThemeApplyToAllRequested,
            this, [this](const ui::themes::TerminalTheme &t) {
        ui::themes::TerminalThemeManager::instance().registerTheme(t);
        for (Session *session : m_sessions) {
            applyThemeToSession(session, t.id);
        }
        m_currentSession.setTerminalThemeId(t.id);
    });

    // Refresh agent providers on all sessions when config changes
    connect(&dlg, &SettingsDialog::agentConfigChanged,
            this, &MainWindow::refreshAgentProviders);

    dlg.exec();
}