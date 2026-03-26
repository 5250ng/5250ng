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
    QMenu *mcpMenu = fileMenu->addMenu("&MCP Server");
    m_mcpEnableAction = mcpMenu->addAction("&Enabled", this, &MainWindow::onMcpToggleEnabled);
    m_mcpEnableAction->setCheckable(true);
    m_mcpEnableAction->setChecked(false); // Updated in MainWindow constructor after config load
    mcpMenu->addAction("Show &Logs...", this, &MainWindow::onMcpShowLogs);
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
    m_hotspotsAction = viewMenu->addAction("&Hotspots", this, &MainWindow::onToggleHotspots);
    m_hotspotsAction->setCheckable(true);
    viewMenu->addSeparator();
    m_agentPanelAction = viewMenu->addAction("&Agent Panel", this, &MainWindow::onToggleAgentPanel);
    m_agentPanelAction->setCheckable(true);
    m_agentPanelAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    viewMenu->addSeparator();
    viewMenu->addAction("History &Back", this, &MainWindow::onHistoryBack);
    viewMenu->addAction("History &Forward", this, &MainWindow::onHistoryForward);
    viewMenu->addAction("&Exit History", this, &MainWindow::onHistoryExit);

    // Scripts menu
    QMenu *scriptsMenu = bar->addMenu("&Scripts");
    m_scriptRecordAction = scriptsMenu->addAction("&Record", this, &MainWindow::onRecordScript);
    m_scriptRecordAction->setCheckable(true);
    m_scriptStopAction = scriptsMenu->addAction("&Stop", this, &MainWindow::onStopExecution);
    m_scriptStopAction->setEnabled(false);
    scriptsMenu->addSeparator();
    m_scriptsSubmenu = scriptsMenu->addMenu("S&cripts");
    connect(m_scriptsSubmenu, &QMenu::aboutToShow, this, &MainWindow::rebuildScriptsSubmenu);
    scriptsMenu->addSeparator();
    scriptsMenu->addAction("&New Script...", this, &MainWindow::onNewScript);
    scriptsMenu->addAction("&Import Script...", this, &MainWindow::onImportScript);
    scriptsMenu->addAction("&Open Scripts Folder", this, &MainWindow::onOpenScriptsFolder);

    // Tools menu
    QMenu *toolsMenu = bar->addMenu("&Tools");
    m_fileTransferAction = toolsMenu->addAction("&File Transfer...", this, &MainWindow::onFileTransfer);
    m_fileTransferAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_fileTransferAction->setEnabled(false);
    toolsMenu->addSeparator();
    toolsMenu->addAction("&Take a Screenshot", this, &MainWindow::onTakeScreenshot);
    m_cursorRulesAction = toolsMenu->addAction("Show &cursor rules", this, &MainWindow::onToggleCursorRules);
    m_cursorRulesAction->setCheckable(true);
    m_sessionLoggingAction = toolsMenu->addAction("Session &Logging", this, &MainWindow::onToggleSessionLogging);
    m_sessionLoggingAction->setCheckable(true);
    toolsMenu->addSeparator();
    // Match and Replace submenu
    QMenu *matchReplaceMenu = toolsMenu->addMenu("Match and &Replace");
    m_matchReplaceEnableAction = matchReplaceMenu->addAction("&Enable", this, &MainWindow::onToggleMatchReplace);
    m_matchReplaceEnableAction->setCheckable(true);
    matchReplaceMenu->addAction("Edit &Patterns...", this, &MainWindow::onEditMatchReplacePatterns);
    // Advanced submenu
    QMenu *advancedMenu = toolsMenu->addMenu("&Advanced");
    m_showFieldProtectionAction = advancedMenu->addAction("Show &field protection", this, &MainWindow::onToggleFieldProtection);
    m_showFieldProtectionAction->setCheckable(true);
    m_showInputFieldsAction = advancedMenu->addAction("Show &input fields", this, &MainWindow::onToggleInputFields);
    m_showInputFieldsAction->setCheckable(true);
    m_showCellGridAction = advancedMenu->addAction("Show cell &grid", this, &MainWindow::onToggleCellGrid);
    m_showCellGridAction->setCheckable(true);

    // Help menu
    QMenu *helpMenu = bar->addMenu("&Help");
    QMenu *debugMenu = helpMenu->addMenu("&Debug");
    debugMenu->addAction("&View session logs", this, &MainWindow::onViewSessionLogs);
    helpMenu->addSeparator();
    helpMenu->addAction("&About", this, &MainWindow::onAbout);
}
