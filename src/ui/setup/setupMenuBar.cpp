#include "../main_window.h"
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

/**
 * Build and initialize the main window menu bar.
 *
 * Menus and actions:
 * - File:
 *   - Connect... (Ctrl+N): open the connection dialog
 *   - Disconnect: terminate the current session
 *   - Settings...: open the settings dialog (Application Theme, 5250 Theme)
 *   - Exit (Ctrl+Q): quit the application
 * - Session:
 *   - New (Add Tab): create a new session tab
 *   - Open saved session (submenu): dynamically populated on aboutToShow and
 *     wired so selecting an entry triggers opening that saved session
 *   - Theme (submenu): quick terminal theme picker
 * - Tools:
 *   - Take a Screenshot: capture the active tab as a PNG
 * - Help:
 *   - About: show application information
 *
 * Signals:
 * - The "Open saved session" submenu connects aboutToShow to rebuild the list
 *   (rebuildQuickOpenMenu) and triggered to open the chosen session
 *   (onSavedSessionChosen).
 */
void MainWindow::setupMenuBar() {
    QMenuBar *bar = titleBar()->menuBar();
    // File menu
    QMenu *fileMenu = bar->addMenu("&File");
    m_connectAction =
        fileMenu->addAction("&Connect...", this, &MainWindow::onConnect);
    m_connectAction->setShortcut(QKeySequence::New);
    m_disconnectAction =
        fileMenu->addAction("&Disconnect", this, &MainWindow::onDisconnect);
    m_disconnectAction->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction("E&xit", this, &QWidget::close);
    m_exitAction->setShortcut(QKeySequence::Quit);

    // Session menu
    QMenu *sessionMenu = bar->addMenu("&Session");
    QAction *newSessionAction =
        sessionMenu->addAction("&New", this, &MainWindow::onNewSession);
    newSessionAction->setShortcut(QKeySequence::AddTab);
    // Quick open saved sessions submenu inside Session menu
    m_quickOpenMenu = sessionMenu->addMenu("Open saved session");
    connect(m_quickOpenMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildQuickOpenMenu);
    connect(m_quickOpenMenu, &QMenu::triggered, this, &MainWindow::onSavedSessionChosen);
    // Quick theme switching submenu
    m_quickThemeMenu = sessionMenu->addMenu("Theme");
    connect(m_quickThemeMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildQuickThemeMenu);
    connect(m_quickThemeMenu, &QMenu::triggered, this, &MainWindow::onQuickThemeChosen);

    // Tools menu
    QMenu *toolsMenu = bar->addMenu("&Tools");
    toolsMenu->addAction("&Take a Screenshot", this, &MainWindow::onTakeScreenshot);
    m_cursorRulesAction = toolsMenu->addAction("Show &cursor rules", this, &MainWindow::onToggleCursorRules);
    m_cursorRulesAction->setCheckable(true);
    // Advanced submenu
    QMenu *advancedMenu = toolsMenu->addMenu("&Advanced");
    m_showFieldProtectionAction = advancedMenu->addAction("Show &field protection", this, &MainWindow::onToggleFieldProtection);
    m_showFieldProtectionAction->setCheckable(true);
    m_showInputFieldsAction = advancedMenu->addAction("Show &input fields", this, &MainWindow::onToggleInputFields);
    m_showInputFieldsAction->setCheckable(true);

    // Help menu
    QMenu *helpMenu = bar->addMenu("&Help");
    QMenu *debugMenu = helpMenu->addMenu("&Debug");
    debugMenu->addAction("&View session logs", this, &MainWindow::onViewSessionLogs);
    helpMenu->addSeparator();
    helpMenu->addAction("&About", this, &MainWindow::onAbout);
}
