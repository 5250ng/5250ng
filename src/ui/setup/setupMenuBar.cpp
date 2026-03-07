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
    m_reconnectAction =
        fileMenu->addAction("&Reconnect", this, &MainWindow::onReconnect);
    m_reconnectAction->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", this, &MainWindow::onOpenSettings);
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction("E&xit", this, &QWidget::close);
    m_exitAction->setShortcut(QKeySequence::Quit);

    // Edit menu
    QMenu *editMenu = bar->addMenu("&Edit");
    editMenu->addAction("&Copy", this, &MainWindow::onEditCopy);
    editMenu->addAction("&Paste", this, &MainWindow::onEditPaste);
    editMenu->addSeparator();
    editMenu->addAction("Select &All", this, &MainWindow::onEditSelectAll);

    // Session menu
    QMenu *sessionMenu = bar->addMenu("&Session");
    QAction *newSessionAction =
        sessionMenu->addAction("&New", this, &MainWindow::onNewSession);
    newSessionAction->setShortcut(QKeySequence::AddTab);
    m_duplicateAction =
        sessionMenu->addAction("&Duplicate Session", this, &MainWindow::onDuplicateSession);
    m_duplicateAction->setEnabled(false);
    sessionMenu->addSeparator();
    // Quick open saved sessions submenu inside Session menu
    m_quickOpenMenu = sessionMenu->addMenu("Open saved session");
    connect(m_quickOpenMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildQuickOpenMenu);
    connect(m_quickOpenMenu, &QMenu::triggered, this, &MainWindow::onSavedSessionChosen);
    // Quick theme switching submenu
    m_quickThemeMenu = sessionMenu->addMenu("Theme");
    connect(m_quickThemeMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildQuickThemeMenu);
    connect(m_quickThemeMenu, &QMenu::triggered, this, &MainWindow::onQuickThemeChosen);

    // View menu
    QMenu *viewMenu = bar->addMenu("&View");
    m_fullscreenAction = viewMenu->addAction("&Fullscreen", this, &MainWindow::onToggleFullscreen);
    m_fullscreenAction->setCheckable(true);

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
