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

#include "core/match_replace_engine.h"
#include "core/macro_recorder.h"
#include <5250script/script_executor.h>
#include "core/session_logger.h"
#include "logger/logger.h"
#include "network/tn5250_qt/client/client.h"
#include "network/tn5250_qt/client/decoder_adapter.h"
#include "session/config.h"
#include "session/worker.h"
#include "ui/dialogs/connect_dialog.h"
#include "ui/dialogs/file_transfer_dialog.h"
#include "ui/rendering/tn5250_command_handler.h"
#include "ui/widgets/Frameless/BaseFramelessWindow.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250TerminalView.h"
#include "ui/widgets/QCRTOverlayWidget/QCRTOverlayWidget.h"
#include "ui/widgets/QAgentPanelWidget/QAgentPanelWidget.h"
#include "ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.h"
#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>

class MainWindow : public ui::widgets::BaseFramelessWindow {
    Q_OBJECT

  public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Auto-connect on startup
    void autoConnect(const session::SessionConfig &config);

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

  private slots:
    void onConnect();
    void onDisconnect();
    void onConnectRequested(const session::SessionConfig &config);
    void onLogMessage(logger::LogLevel level, const QString &message);
    void onCurrentTabChanged(int index);
    void onTabMoved(int from, int to);
    void onCloseTabRequested(int index);
    void onRenameTabRequested(int index);
    void onNewSession();
    void onAbout();
    void onTakeScreenshot();
    void onToggleCursorRules();
    void onToggleFieldProtection();
    void onToggleInputFields();
    void onToggleCellGrid();
    void onReconnect();
    void onDuplicateSession();
    void onToggleFullscreen();
    void onEditCopy();
    void onEditPaste();
    void onEditSelectAll();
    void onToggleHotspots();
    void onRecordScript();
    void onStopExecution();
    void rebuildScriptsSubmenu();
    void onRunScript(const QString &path);
    void onEditScript(const QString &path);
    void onDeleteScript(const QString &path);
    void onNewScript();
    void onImportScript();
    void onOpenScriptsFolder();
    void onToggleSessionLogging();
    void onHistoryBack();
    void onHistoryForward();
    void onHistoryExit();
    void updateEmptyState();
    void onQuickOpenChanged(const QString &sessionName);
    void rebuildQuickOpenMenu();
    void fillSessionsCombo(QComboBox *combo, const QString &placeholder);
    void onSavedSessionChosen(QAction *action);
    void onOpenSettings();
    void onToggleAgentPanel();
    void onViewSessionLogs();
    void onFileTransfer();
    void onToggleMatchReplace();
    void onEditMatchReplacePatterns();
    void rebuildQuickThemeMenu();
    void onQuickThemeChosen(QAction *action);

  private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void connectToServer(const session::SessionConfig &config);
    void setActiveSession(int index);
    void openContextMenuForTab(const QPoint &pos);

    struct Session {
        QWidget *container;
        QWidget *terminalContainer = nullptr;
        QSplitter *splitter = nullptr;
        ui::widgets::QAgentPanelWidget *agentPanel = nullptr;
        ui::widgets::Q5250TerminalView *terminalView;
        ui::widgets::Q5250ScreenWidget *displayWidget;
        tn5250::client::DecoderAdapter *parser;
        QThread *thread;
        tn5250::session::Worker *worker;
        ui::widgets::QConnectionStatusWidget *connectionStatus;
        QLabel *coordinatesLabel;
        QLabel *kbdStateLabel;     // Keyboard lock/insert indicator
        QLabel *systemNameLabel;   // Detected system name
        QLabel *historyLabel;      // Screen history position
        QLabel *macroLabel;        // Macro recording/playback indicator
        QWidget *statusBar;
        session::SessionConfig config;
        ui::widgets::ScreenBuffer::SavedState savedScreen;
        ui::rendering::TN5250CommandHandler *commandHandler;
        ui::widgets::QCRTOverlayWidget *crtOverlay;
        // Background image (painted in container's Paint event via event filter)
        QPixmap bgImage;
        QPixmap bgImageScaled;       // Cached scaled version
        QSize bgImageScaledSize;     // Size the cache was generated for
        ui::themes::TerminalTheme::BackgroundImageLayout bgImageLayout =
            ui::themes::TerminalTheme::Stretch;
        double bgImageOpacity = 1.0;
        core::MacroRecorder *macroRecorder = nullptr;
        core::scripting::ScriptExecutor *scriptExecutor = nullptr;
        core::SessionLogger *sessionLogger = nullptr;
        core::MatchReplaceEngine *matchReplace = nullptr;
    };

    void runStartupScript(Session *session, const QString &scriptPath);
    void applyThemeToSession(Session *session, const QString &themeId);

    QTabWidget *m_tabWidget;
    QVector<Session *> m_sessions;
    int m_activeIndex;
    QWidget *m_emptyPlaceholder;
    QMenu *m_quickOpenMenu;
    QMenu *m_quickThemeMenu;
    QComboBox *m_emptyOpenCombo;

    // Convenience pointers to the active session
    ui::widgets::Q5250ScreenWidget *m_displayWidget;
    tn5250::client::DecoderAdapter *m_parser;
    session::SessionConfig m_currentSession;

    // UI elements
    QAction *m_connectAction;
    QAction *m_disconnectAction;
    QAction *m_exitAction;
    QAction *m_reconnectAction;
    QAction *m_duplicateAction;
    QAction *m_fullscreenAction;
    QAction *m_hotspotsAction;
    QAction *m_scriptRecordAction;
    QAction *m_scriptStopAction;
    QMenu *m_scriptsSubmenu;
    QAction *m_sessionLoggingAction;
    QAction *m_cursorRulesAction;
    QAction *m_showFieldProtectionAction;
    QAction *m_showInputFieldsAction;
    QAction *m_showCellGridAction;
    QAction *m_agentPanelAction;
    QAction *m_fileTransferAction;
    QAction *m_matchReplaceEnableAction;
    QLabel *m_cursorCoordinates; // Cursor position display (row/col)
    ui::widgets::QConnectionStatusWidget *m_globalConnectionStatus; // Global bottom-bar status

    bool m_connected;
    QTimer m_resizeLogTimer;            // Debounce resize logging

    void updateCursorCoordinates();     // Update cursor position display
    void updateCursorCoordinatesFont(); // Update cursor coordinates font to match
                                        // screen
};