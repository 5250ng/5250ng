#pragma once

#include "logger/logger.h"
#include "network/tn5250/client/client.h"
#include "network/tn5250/client/decoder.h"
#include "session/config.h"
#include "session/worker.h"
#include "ui/dialogs/connect_dialog.h"
#include "ui/rendering/tn5250_command_handler.h"
#include "ui/widgets/Frameless/BaseFramelessWindow.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250TerminalView.h"
#include "ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.h"
#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>
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
    void updateEmptyState();
    void onQuickOpenChanged(const QString &sessionName);
    void rebuildQuickOpenMenu();
    void fillSessionsCombo(QComboBox *combo, const QString &placeholder);
    void onSavedSessionChosen(QAction *action);
    void onOpenSettings();
    void onViewSessionLogs();
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
        ui::widgets::Q5250TerminalView *terminalView;
        ui::widgets::Q5250ScreenWidget *displayWidget;
        tn5250::client::Decoder *parser;
        QThread *thread;
        tn5250::session::Worker *worker;
        ui::widgets::QConnectionStatusWidget *connectionStatus;
        QLabel *coordinatesLabel;
        QWidget *statusBar;
        session::SessionConfig config;
        ui::widgets::ScreenBuffer::SavedState savedScreen;
        ui::rendering::TN5250CommandHandler *commandHandler;
    };

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
    tn5250::client::Decoder *m_parser;
    session::SessionConfig m_currentSession;

    // UI elements
    QAction *m_connectAction;
    QAction *m_disconnectAction;
    QAction *m_exitAction;
    QAction *m_cursorRulesAction;
    QAction *m_showFieldProtectionAction;
    QAction *m_showInputFieldsAction;
    QLabel *m_cursorCoordinates; // Cursor position display (row/col)

    bool m_connected;

    void updateCursorCoordinates();     // Update cursor position display
    void updateCursorCoordinatesFont(); // Update cursor coordinates font to match
                                        // screen
};