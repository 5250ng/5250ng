#pragma once

#include "logger/logger.h"
#include "network/tn5250/client/client.h"
#include "network/tn5250/client/decoder.h"
#include "session/config.h"
#include "session/worker.h"
#include "ui/dialogs/connect_dialog.h"
#include "ui/widgets/Frameless/BaseFramelessWindow.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
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

  private slots:
    void onConnect();
    void onDisconnect();
    void onConnectRequested(const session::SessionConfig &config);
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(const QString &error);
    void onDataReceived(const QByteArray &data);
    void onInputReady(const QByteArray &data);
    void onLogMessage(logger::LogLevel level, const QString &message);
    void onCurrentTabChanged(int index);
    void onTabMoved(int from, int to);
    void onCloseTabRequested(int index);
    void onRenameTabRequested(int index);
    void onNewSession();
    void onSessionSettings();
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

  private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void connectToServer(const session::SessionConfig &config);
    void updateConnectionStatus(bool connected);
    void
    updateStatusIndicator(tn5250::client::TN5250Client::ConnectionState state);
    void handleTN5250Command(tn5250::client::TN5250Command cmd, const QByteArray &data);
    void handleStructuredField(tn5250::client::StructuredFieldType type, const QByteArray &data);
    void handleRawScreenData(const QByteArray &data);
    void onClearScreenRequested();
    void onKeyboardUnlockRequested();
    void onControlCharactersReceived(uint8_t cc1, uint8_t cc2);
    void onSohReceived(uint8_t errorRow, uint8_t ckm1, uint8_t ckm2, uint8_t ckm3);
    void onRollRequested(uint8_t topRow, uint8_t botRow, uint8_t lines, bool up);
    void onWriteErrorCode(const QByteArray &errorCode);
    void onSaveScreenRequested();
    void onClearScreenAlternateRequested();
    void onClearFormatTableRequested();
    QStringList hexdump(const QByteArray &data);
    void connectSessionSignals();
    void disconnectSessionSignals();
    void setActiveSession(int index);
    void openContextMenuForTab(const QPoint &pos);
    void renderTN5250Stream(const QByteArray &data);
    QByteArray buildFieldResponse(uint8_t aidByte);
    void sendToHost(const QByteArray &data);

    struct Session {
        QWidget *container;
        ui::widgets::Q5250ScreenWidget *displayWidget;
        tn5250::client::TN5250Client *client;
        tn5250::client::Decoder *parser;
        QThread *thread;
        tn5250::session::Worker *worker;
        ui::widgets::QConnectionStatusWidget *connectionStatus;
        QLabel *coordinatesLabel;
        session::SessionConfig config;
        ui::widgets::ScreenBuffer::SavedState savedScreen; // For Save/Restore Screen
    };

    QTabWidget *m_tabWidget;
    QVector<Session *> m_sessions;
    int m_activeIndex;
    QWidget *m_emptyPlaceholder;
    QMenu *m_quickOpenMenu;
    QComboBox *m_emptyOpenCombo;

    // Convenience pointers to the active session
    ui::widgets::Q5250ScreenWidget *m_displayWidget;
    tn5250::client::TN5250Client *m_client;
    tn5250::client::Decoder *m_parser;
    session::SessionConfig m_currentSession;

    // UI elements
    QAction *m_connectAction;
    QAction *m_disconnectAction;
    QAction *m_exitAction;
    QAction *m_showFieldProtectionAction;
    QAction *m_showInputFieldsAction;
    QLabel *m_cursorCoordinates; // Cursor position display (row/col)

    bool m_connected;

    void updateCursorCoordinates();     // Update cursor position display
    void updateCursorCoordinatesFont(); // Update cursor coordinates font to match
                                        // screen
};