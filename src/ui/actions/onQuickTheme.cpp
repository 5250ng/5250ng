#include "ui/main_window.h"
#include "session/manager.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QPainter>
#include <QPixmap>

void MainWindow::rebuildQuickThemeMenu() {
    m_quickThemeMenu->clear();
    auto &mgr = ui::themes::TerminalThemeManager::instance();

    // Get current session's theme id for check mark
    QString currentThemeId;
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        currentThemeId = m_sessions[m_activeIndex]->config.terminalThemeId();
    }

    for (const QString &id : mgr.availableThemes()) {
        ui::themes::TerminalTheme t = mgr.theme(id);

        // Color swatch icon: background color with green center
        QPixmap swatch(16, 16);
        swatch.fill(t.backgroundColor);
        QPainter p(&swatch);
        p.setPen(Qt::NoPen);
        p.setBrush(t.colorGreen);
        p.drawRect(4, 4, 8, 8);
        p.end();

        QString label = t.displayName;
        QAction *action = m_quickThemeMenu->addAction(QIcon(swatch), label);
        action->setData(id);
        action->setCheckable(true);
        action->setChecked(id == currentThemeId);
        if (!t.description.isEmpty()) {
            action->setToolTip(t.description);
        }
    }

}

static void persistSessionTheme(const session::SessionConfig &config) {
    if (config.name().isEmpty() || config.name() == "Current Session"
        || config.name() == "Command Line Session") return;
    session::SessionManager mgr;
    session::SessionConfig saved;
    if (mgr.loadSession(config.name(), saved)) {
        saved.setTerminalThemeId(config.terminalThemeId());
        mgr.saveSession(saved);
    }
}

void MainWindow::onQuickThemeChosen(QAction *action) {
    if (!action) return;
    QString themeId = action->data().toString();
    if (themeId.isEmpty()) return;

    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;

    applyThemeToSession(m_sessions[m_activeIndex], themeId);
    m_currentSession.setTerminalThemeId(themeId);
    persistSessionTheme(m_sessions[m_activeIndex]->config);
}

