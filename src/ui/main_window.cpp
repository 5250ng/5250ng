#include "main_window.h"
#include "logger/logger.h"
#include "session/config.h"
#include "session/manager.h"
#include "ui/dialogs/log_viewer.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250TerminalView.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabBar>
#include <QTimer>
#include <QWidgetAction>
#include <climits>

/**
 * Construct the main application window.
 *
 * Sets up the base window geometry, initializes the UI (central tab widget and
 * empty-state placeholder), menus, and status bar, and wires the logger
 * sink so log messages can surface in the UI.
 *
 * @param parent Optional parent QWidget.
 */
MainWindow::MainWindow(QWidget *parent)
    : ui::widgets::BaseFramelessWindow(parent), m_displayWidget(nullptr), m_client(nullptr),
      m_parser(nullptr), m_cursorCoordinates(nullptr), m_connected(false) {
    setWindowTitle("5250ng");
    resize(1128, 836);

    setupUI();
    setupMenuBar();
    setupStatusBar();

    // Initialize logger
    logger::Logger::instance()->debug("5250ng started");
    connect(logger::Logger::instance(), &logger::Logger::logMessage, this, &MainWindow::onLogMessage);
}

void MainWindow::onViewSessionLogs() {
    tn5250::session::Worker *worker = nullptr;
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        Session *s = m_sessions[m_activeIndex];
        worker = s ? s->worker : nullptr;
    }
    LogViewerDialog *dlg = worker ? new LogViewerDialog(worker, this) : new LogViewerDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
}

/**
 * Destructor.
 *
 * Ensures an active TN5250 connection is cleanly closed when the main window
 * is destroyed.
 */
MainWindow::~MainWindow() {
    if (m_client) {
        m_client->disconnectFromHost();
    }
}

/**
 * Create a new session tab and connect to a TN5250 host.
 *
 * Builds the per-tab container UI, creates the TN5250 client and protocol
 * parser, wires input/output signals, and initiates the network connection
 * according to the provided session configuration.
 *
 * @param config Session configuration (host, port, TLS, device, rows/cols).
 */
void MainWindow::connectToServer(const session::SessionConfig &config) {
    m_currentSession = config;

    // Create a new session tab
    Session *session = new Session();
    session->container = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(session->container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    // Terminal view: screen + hrule + footer screen(1 row)
    ui::widgets::Q5250TerminalView *terminalView = new ui::widgets::Q5250TerminalView(session->container);
    tabLayout->addWidget(terminalView);
    session->displayWidget = terminalView->screen();
    // Per-tab footer with connection status (left) and cursor coordinates (right)
    QHBoxLayout *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(5, 2, 5, 2);
    footerLayout->setSpacing(6);
    // Per-tab connection status on the left
    session->connectionStatus = new ui::widgets::QConnectionStatusWidget(session->container);
    session->connectionStatus->setState(tn5250::client::TN5250Client::ConnectionState::Disconnected);
    session->connectionStatus->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    footerLayout->addWidget(session->connectionStatus, 0, Qt::AlignLeft | Qt::AlignVCenter);
    footerLayout->addStretch();
    session->coordinatesLabel = new QLabel("0/0", session->container);
    session->coordinatesLabel->setStyleSheet(
        "color: white; background-color: black; padding: 0px;"
    );
    footerLayout->addWidget(session->coordinatesLabel);
    QWidget *footerWidget = new QWidget(session->container);
    footerWidget->setLayout(footerLayout);
    footerWidget->setStyleSheet("background-color: black;");
    footerWidget->setMinimumHeight(22);
    footerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    tabLayout->addWidget(footerWidget);
    // Ensure the display fills remaining space and footer stays at bottom
    tabLayout->setStretch(0, 1); // terminal view grows
    tabLayout->setStretch(1, 0); // footer
    session->container->setLayout(tabLayout);
    session->client = nullptr; // handled by session worker thread
    session->parser = new tn5250::client::Decoder(session->container);
    session->thread = new QThread(this);
    session->worker = new tn5250::session::Worker();
    session->worker->setConfig(config);
    session->worker->moveToThread(session->thread);
    connect(session->thread, &QThread::started, session->worker, &tn5250::session::Worker::start);
    connect(session->thread, &QThread::finished, session->worker, &QObject::deleteLater);
    // Forward worker state to UI
    connect(session->worker, &tn5250::session::Worker::connected, this, &MainWindow::onConnected);
    connect(session->worker, &tn5250::session::Worker::disconnected, this, &MainWindow::onDisconnected);
    connect(session->worker, &tn5250::session::Worker::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(session->worker, &tn5250::session::Worker::stateChanged, this, &MainWindow::updateStatusIndicator);
    // Also update this session's status widget directly regardless of active tab
    connect(session->worker, &tn5250::session::Worker::stateChanged, this, [session](tn5250::client::TN5250Client::ConnectionState st) {
        if (session->connectionStatus) {
            session->connectionStatus->setState(st);
        }
    });
    // App data: feed this session's parser directly
    connect(session->worker, &tn5250::session::Worker::appData, this, [this, session](const QByteArray &bytes) {
        if (session->parser) {
            session->parser->parseData(bytes);
        } }, Qt::QueuedConnection);
    session->config = config;

    int newIndex = m_tabWidget->addTab(
        session->container,
        QString("%1:%2").arg(config.hostname()).arg(config.port())
    );
    // Custom tab header with title + close button
    {
        QWidget *tabHeader = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(tabHeader);
        h->setContentsMargins(8, 0, 4, 0);
        h->setSpacing(6);
        QLabel *title = new QLabel(QString("%1:%2").arg(config.hostname()).arg(config.port()), tabHeader);
        title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QPushButton *closeBtn = new QPushButton(QString::fromUtf8("✕"), tabHeader);
        closeBtn->setFlat(true);
        closeBtn->setFixedSize(16, 16);
        closeBtn->setToolTip("Close");
        h->addWidget(title, 1);
        h->addWidget(closeBtn, 0, Qt::AlignRight | Qt::AlignVCenter);
        m_tabWidget->tabBar()->setTabButton(newIndex, QTabBar::RightSide, nullptr);
        m_tabWidget->setTabText(newIndex, QString()); // remove default text
        m_tabWidget->tabBar()->setTabButton(newIndex, QTabBar::LeftSide, tabHeader);
        QWidget *page = session->container;
        connect(closeBtn, &QPushButton::clicked, this, [this, page]() {
            int idx = m_tabWidget->indexOf(page);
            if (idx >= 0) {
                onCloseTabRequested(idx);
            }
        });
    }
    m_sessions.insert(newIndex, session);
    m_tabWidget->setCurrentIndex(newIndex);

    // Setup display size from session config
    session->displayWidget->setScreenSize(config.screenRows(), config.screenCols());

    // Wire display input
    connect(session->displayWidget, &ui::widgets::Q5250ScreenWidget::inputReady, this, &MainWindow::onInputReady);
    if (session->displayWidget->screenBuffer()) {
        connect(session->displayWidget->screenBuffer(), &ui::widgets::ScreenBuffer::cursorMoved, this, &MainWindow::updateCursorCoordinates);
    }

    // Set active pointers and connect signals for this session
    setActiveSession(newIndex);

    // Create command handler for this session
    session->commandHandler = new ui::rendering::TN5250CommandHandler(session->container);
    session->commandHandler->setDisplayWidget(session->displayWidget);
    session->commandHandler->setSendToHostCallback([this](const QByteArray &data) {
        sendToHost(data);
    });
    session->commandHandler->connectDecoder(m_parser);
    // Wire save screen (needs access to per-session state)
    connect(m_parser, &tn5250::client::Decoder::saveScreenRequested, this, &MainWindow::onSaveScreenRequested);

    // Start the session thread (which triggers the worker start)
    session->thread->start();
    // Status indicator will be updated via worker stateChanged signal
    updateEmptyState();
}

void MainWindow::onToggleCursorRules() {
    if (m_displayWidget) {
        m_displayWidget->toggleCursorRules();
    }
}

void MainWindow::onToggleFieldProtection() {
    if (m_displayWidget) {
        m_displayWidget->toggleFieldProtection();
        m_showFieldProtectionAction->setChecked(m_displayWidget->showFieldProtection());
    }
}

void MainWindow::onToggleInputFields() {
    if (m_displayWidget) {
        m_displayWidget->toggleInputFields();
        m_showInputFieldsAction->setChecked(m_displayWidget->showInputFields());
    }
}
/**
 * Update UI widgets that reflect whether a session is connected.
 *
 * Enables/disables Connect/Disconnect actions and updates the status text.
 *
 * @param connected True if the active session is connected.
 */
void MainWindow::updateConnectionStatus(bool connected) {
    m_connected = connected;
    m_connectAction->setEnabled(!connected);
    m_disconnectAction->setEnabled(connected);

    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        Session *s = m_sessions[m_activeIndex];
        if (s && s->connectionStatus) {
            if (connected) {
                s->connectionStatus->setStatusText(QString("Connected to %1:%2")
                                                       .arg(m_currentSession.hostname())
                                                       .arg(m_currentSession.port()));
            } else {
                s->connectionStatus->setStatusText("Not connected");
            }
        }
    }
}

/**
 * Update the small round status indicator and text for the given state.
 *
 * The indicator color and tooltip are driven by the current theme using a
 * dynamic "state" property. This function also sets the status text message.
 *
 * @param state Current TN5250 client connection state.
 */
void MainWindow::updateStatusIndicator(
    tn5250::client::TN5250Client::ConnectionState state
) {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) {
        return;
    }
    Session *s = m_sessions[m_activeIndex];
    if (!s || !s->connectionStatus)
        return;
    // Preserve detailed text when connected
    if (state == tn5250::client::TN5250Client::ConnectionState::Connected &&
        m_connected && !m_currentSession.hostname().isEmpty()) {
        s->connectionStatus->setStatusText(QString("Connected to %1:%2")
                                               .arg(m_currentSession.hostname())
                                               .arg(m_currentSession.port()));
    } else if (state == tn5250::client::TN5250Client::ConnectionState::Disconnected ||
               state == tn5250::client::TN5250Client::ConnectionState::Error) {
        s->connectionStatus->setStatusText("Not connected");
    } else if (state == tn5250::client::TN5250Client::ConnectionState::Negotiating) {
        s->connectionStatus->setStatusText("Waiting for system");
    } else if (state == tn5250::client::TN5250Client::ConnectionState::Connecting) {
        s->connectionStatus->setStatusText("Connecting");
    } else {
        s->connectionStatus->setStatusText("Ready");
    }
    s->connectionStatus->setState(state);
}

/**
 * Schedule an automatic connection with a short delay.
 *
 * Useful on application startup to let the UI settle before connecting.
 *
 * @param config Session configuration to use for connecting.
 */
void MainWindow::autoConnect(const session::SessionConfig &config) {
    // Connect after a short delay to ensure UI is ready
    QTimer::singleShot(100, this, [this, config]() { connectToServer(config); });
}

// handleTN5250Command, handleStructuredField, handleRawScreenData
// are now in ui::rendering::TN5250CommandHandler

/**
 * Update the coordinates label with the current cursor position.
 *
 * Reads the screen buffer cursor, converts to 1-based row/column, and updates
 * the per-tab footer label.
 */
void MainWindow::updateCursorCoordinates() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer() ||
        !m_cursorCoordinates) {
        return;
    }

    // Update font to match display widget (in case it changed due to resize)
    updateCursorCoordinatesFont();

    QPoint cursorPos = m_displayWidget->screenBuffer()->cursorPosition();
    // Display as 1-based (row/col) as terminals typically do
    int row = cursorPos.y() + 1;
    int col = cursorPos.x() + 1;

    m_cursorCoordinates->setText(QString("%1/%2").arg(row).arg(col));
}

/**
 * Keep the coordinates label font in sync with the display widget.
 *
 * Ensures the label uses the display font and reserves enough space for the
 * largest expected coordinate text.
 */
void MainWindow::updateCursorCoordinatesFont() {
    if (!m_displayWidget || !m_cursorCoordinates) {
        return;
    }

    // Use the same font as the display widget (scaled font)
    QFont displayFont = m_displayWidget->font();
    m_cursorCoordinates->setFont(displayFont);

    // Set minimum size to match one character cell
    QFontMetrics fm(displayFont);
    int charWidth = fm.horizontalAdvance('M');
    int charHeight = fm.height();
    m_cursorCoordinates->setMinimumSize(charWidth * 6,
                                        charHeight); // Enough for "999/999"
    m_cursorCoordinates->setMaximumHeight(charHeight);
}

/**
 * Update convenience pointers and UI to target the session at index.
 *
 * @param index Index in the sessions vector, or -1 to clear active state.
 */
void MainWindow::setActiveSession(int index) {
    if (index < 0 || index >= m_sessions.size()) {
        m_activeIndex = -1;
        m_displayWidget = nullptr;
        m_client = nullptr;
        m_parser = nullptr;
        return;
    }
    m_activeIndex = index;
    Session *s = m_sessions[index];
    m_displayWidget = s->displayWidget;
    m_client = s->client;
    m_parser = s->parser;
    m_cursorCoordinates = s->coordinatesLabel;
    m_currentSession = s->config;
    updateCursorCoordinatesFont();
    updateCursorCoordinates();
}

/**
 * Connect client and parser signals for the currently active session.
 *
 * No-ops if required objects are missing.
 */
void MainWindow::connectSessionSignals() {
    if (!m_client || !m_parser) {
        return;
    }
    connect(m_client, &tn5250::client::TN5250Client::connected, this, &MainWindow::onConnected);
    connect(m_client, &tn5250::client::TN5250Client::disconnected, this, &MainWindow::onDisconnected);
    connect(m_client, &tn5250::client::TN5250Client::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_client, &tn5250::client::TN5250Client::dataReceived, this, &MainWindow::onDataReceived);
    connect(m_client, &tn5250::client::TN5250Client::stateChanged, this, &MainWindow::updateStatusIndicator);

    // Parser signals are handled by the per-session TN5250CommandHandler
    // (wired in connectToServer)
    connect(m_parser, &tn5250::client::Decoder::saveScreenRequested, this, &MainWindow::onSaveScreenRequested);
}

/**
 * Disconnect client and parser signals from the main window.
 *
 * No-ops if required objects are missing.
 */
void MainWindow::disconnectSessionSignals() {
    if (!m_client || !m_parser) {
        return;
    }
    m_client->disconnect(this);
    m_parser->disconnect(this);
}

/**
 * Intercept context menu events on the tab bar to support per-tab actions.
 *
 * @param obj   The watched object.
 * @param event The incoming event.
 * @return true if the event was handled, false to propagate.
 */
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_tabWidget->tabBar() && event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *ce = static_cast<QContextMenuEvent *>(event);
        int tabIndex = m_tabWidget->tabBar()->tabAt(ce->pos());
        if (tabIndex >= 0) {
            QMenu menu(this);
            QAction *rename = menu.addAction("Rename");
            QAction *close = menu.addAction("Close");
            QAction *chosen = menu.exec(ce->globalPos());
            if (chosen == rename) {
                onRenameTabRequested(tabIndex);
            } else if (chosen == close) {
                onCloseTabRequested(tabIndex);
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// onClearScreenRequested, onKeyboardUnlockRequested, onControlCharactersReceived,
// processDeferredCC2, onSohReceived, onRollRequested, onWriteErrorCode
// are now in ui::rendering::TN5250CommandHandler

void MainWindow::onSaveScreenRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        m_sessions[m_activeIndex]->savedScreen = m_displayWidget->screenBuffer()->saveState();
        logger::Logger::instance()->debug("MainWindow: Screen saved");
    }
}

// onClearScreenAlternateRequested, onClearFormatTableRequested
// are now in ui::rendering::TN5250CommandHandler

/**
 * Show or hide the empty-state placeholder when there are no tabs.
 *
 * Also refreshes the quick-open dropdown when the placeholder is visible.
 */
void MainWindow::updateEmptyState() {
    bool hasTabs = !m_sessions.isEmpty();
    m_tabWidget->setVisible(hasTabs);
    m_emptyPlaceholder->setVisible(!hasTabs);
    if (!hasTabs && m_emptyOpenCombo) {
        fillSessionsCombo(m_emptyOpenCombo, "(Open saved session)");
    }
}

/**
 * Rebuild the "Open saved session" submenu with current sessions list.
 *
 * Populates actions that, when triggered, open the corresponding session.
 */
void MainWindow::rebuildQuickOpenMenu() {
    if (!m_quickOpenMenu)
        return;
    m_quickOpenMenu->clear();
    session::SessionManager mgr(this);
    QStringList sessions = mgr.listSessions();
    sessions.sort(Qt::CaseInsensitive);
    if (sessions.isEmpty()) {
        QAction *none = m_quickOpenMenu->addAction("(No saved sessions)");
        none->setEnabled(false);
        return;
    }
    for (const QString &name : sessions) {
        QAction *a = m_quickOpenMenu->addAction(name);
        a->setData(name);
    }
}

// renderTN5250Stream is now in ui::rendering::TN5250StreamRenderer
// buildFieldResponse is now in ui::rendering::TN5250CommandHandler

/**
 * Populate a QComboBox with the list of saved sessions.
 *
 * @param combo        Target combo box to fill.
 * @param placeholder  First item text shown when no session is chosen.
 */
void MainWindow::fillSessionsCombo(QComboBox *combo, const QString &placeholder) {
    if (!combo)
        return;
    QString current = combo->currentText();
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(placeholder);
    session::SessionManager mgr(this);
    QStringList sessions = mgr.listSessions();
    sessions.sort(Qt::CaseInsensitive);
    for (const QString &name : sessions) {
        combo->addItem(name);
    }
    int idx = combo->findText(current);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    } else {
        combo->setCurrentIndex(0);
    }
    combo->blockSignals(false);
}

void MainWindow::sendToHost(const QByteArray &data) {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) {
        return;
    }
    Session *s = m_sessions[m_activeIndex];
    if (s && s->worker) {
        QMetaObject::invokeMethod(s->worker, "sendInput", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, data));
    }
}