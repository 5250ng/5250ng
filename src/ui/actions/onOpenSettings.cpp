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

    dlg.exec();
}