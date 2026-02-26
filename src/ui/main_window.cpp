#include "main_window.h"
#include "logger/logger.h"
#include "session/config.h"
#include "session/manager.h"
#include "ui/dialogs/log_viewer.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250TerminalView.h"
#include "utils/hex/hex.h"
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
    // Connect parser signals directly (client is handled in worker)
    connect(m_parser, &tn5250::client::Decoder::commandReceived, this, &MainWindow::handleTN5250Command);
    connect(m_parser, &tn5250::client::Decoder::structuredFieldReceived, this, &MainWindow::handleStructuredField);
    connect(m_parser, &tn5250::client::Decoder::rawScreenDataReceived, this, &MainWindow::handleRawScreenData);
    connect(m_parser, &tn5250::client::Decoder::clearScreenRequested, this, &MainWindow::onClearScreenRequested);
    connect(m_parser, &tn5250::client::Decoder::keyboardUnlockRequested, this, &MainWindow::onKeyboardUnlockRequested);
    connect(m_parser, &tn5250::client::Decoder::controlCharactersReceived, this, &MainWindow::onControlCharactersReceived);
    connect(m_parser, &tn5250::client::Decoder::sohReceived, this, &MainWindow::onSohReceived);
    connect(m_parser, &tn5250::client::Decoder::rollRequested, this, &MainWindow::onRollRequested);
    connect(m_parser, &tn5250::client::Decoder::writeErrorCodeRequested, this, &MainWindow::onWriteErrorCode);
    connect(m_parser, &tn5250::client::Decoder::saveScreenRequested, this, &MainWindow::onSaveScreenRequested);
    connect(m_parser, &tn5250::client::Decoder::clearScreenAlternateRequested, this, &MainWindow::onClearScreenAlternateRequested);
    connect(m_parser, &tn5250::client::Decoder::clearFormatTableRequested, this, &MainWindow::onClearFormatTableRequested);

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

/**
 * Handle an incoming TN5250 command from the protocol parser.
 *
 * Decodes commands and writes data into the screen buffer, including parsing
 * Start Field markers to update per-character attributes where applicable.
 *
 * @param cmd  The TN5250 command identifier.
 * @param data Raw command payload bytes.
 */
void MainWindow::handleTN5250Command(tn5250::client::TN5250Command cmd, const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    logger::Logger::instance()->debug(
        QString("MainWindow: Handling TN5250 command: %1, data size: %2")
            .arg(static_cast<int>(cmd))
            .arg(data.size())
    );

    switch (cmd) {
    case tn5250::client::TN5250Command::READ_MDT_FIELDS: {
        // Host sets up for input: send modified fields when user presses an AID key.
        // Do NOT respond immediately — the user's AID key press triggers the response.
        logger::Logger::instance()->debug("MainWindow: READ_MDT_FIELDS - awaiting user AID key");
        break;
    }

    case tn5250::client::TN5250Command::ERASE_WRITE:
    case tn5250::client::TN5250Command::ERASE_WRITE_ALTERNATE: {
        // These commands are not emitted by our Decoder (WTD data goes through
        // rawScreenDataReceived). Log and ignore.
        logger::Logger::instance()->debug(
            QString("MainWindow: ERASE_WRITE variant (not used in data path)")
        );
        break;
    }

    case tn5250::client::TN5250Command::READ_MODIFY:
    case tn5250::client::TN5250Command::READ_MODIFY_WRITE: {
        logger::Logger::instance()->debug(
            QString("MainWindow: READ_MODIFY command (not yet implemented)")
        );
        break;
    }

    case tn5250::client::TN5250Command::READ_INPUT_FIELDS: {
        // Host sets up for input: send ALL input fields when user presses an AID key.
        // Do NOT respond immediately — the user's AID key press triggers the response.
        logger::Logger::instance()->debug("MainWindow: READ_INPUT_FIELDS - awaiting user AID key");
        break;
    }

    case tn5250::client::TN5250Command::READ_IMMEDIATE: {
        // Host requests immediate response without waiting for AID
        sendToHost(buildFieldResponse(0x7D));
        logger::Logger::instance()->debug("MainWindow: READ_IMMEDIATE - sent immediate response");
        break;
    }

    case tn5250::client::TN5250Command::WRITE_STRUCTURED_FIELD: {
        logger::Logger::instance()->debug("MainWindow: WRITE_STRUCTURED_FIELD "
                                          "(handled by structuredFieldReceived)");
        break;
    }

    default:
        logger::Logger::instance()->warning(
            QString("MainWindow: Unknown TN5250 command: %1")
                .arg(static_cast<int>(cmd))
        );
        break;
    }
}

/**
 * Handle an incoming TN5250 structured field block.
 *
 * Certain screens and control sequences are conveyed as structured fields.
 * This function handles known types and logs others for future support.
 *
 * @param type Structured field type identifier.
 * @param data Raw structured field data bytes.
 */
void MainWindow::handleStructuredField(tn5250::client::StructuredFieldType type, const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    logger::Logger::instance()->debug(
        QString("MainWindow: Handling structured field type: %1, data size: %2")
            .arg(static_cast<int>(type))
            .arg(data.size())
    );

    switch (type) {
    case tn5250::client::StructuredFieldType::OUTBOUND_5250_DS: {
        // OUTBOUND_5250_DS contains the actual screen data
        // Format: [flags(1)] [row(1)] [col(1)] [data...]
        if (data.size() >= 3) {
            QByteArray screenData = data.mid(3);
            renderTN5250Stream(screenData);
        }
        break;
    }

    case tn5250::client::StructuredFieldType::SCS: {
        logger::Logger::instance()->debug("MainWindow: SCS structured field (not yet implemented)");
        break;
    }

    default:
        logger::Logger::instance()->debug(
            QString("MainWindow: Unhandled structured field type: %1")
                .arg(static_cast<int>(type))
        );
        break;
    }
}

/**
 * Handle raw TN5250 screen data (display orders extracted by the Decoder).
 *
 * The Decoder has already stripped the GDS record header and ESC/CC/ctrl bytes.
 * This data contains only display orders (SBA, SF, RA, IC, MC) and EBCDIC characters.
 *
 * @param data Display orders and EBCDIC data bytes.
 */
void MainWindow::handleRawScreenData(const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    logger::Logger::instance()->debug(
        QString("MainWindow: Handling raw screen data - %1 bytes")
            .arg(data.size())
    );
    std::vector<uint8_t> dumpBuf(reinterpret_cast<const uint8_t *>(data.constData()), reinterpret_cast<const uint8_t *>(data.constData()) + data.size());
    std::vector<std::string> hexLines = utils::hex::hexdump(dumpBuf);
    for (const std::string &line : hexLines) {
        logger::Logger::instance()->debug(QString::fromStdString(line));
    }

    // Data is already extracted display orders — render directly
    renderTN5250Stream(data);
}

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

    connect(m_parser, &tn5250::client::Decoder::commandReceived, this, &MainWindow::handleTN5250Command);
    connect(m_parser, &tn5250::client::Decoder::structuredFieldReceived, this, &MainWindow::handleStructuredField);
    connect(m_parser, &tn5250::client::Decoder::rawScreenDataReceived, this, &MainWindow::handleRawScreenData);
    connect(m_parser, &tn5250::client::Decoder::clearScreenRequested, this, &MainWindow::onClearScreenRequested);
    connect(m_parser, &tn5250::client::Decoder::keyboardUnlockRequested, this, &MainWindow::onKeyboardUnlockRequested);
    connect(m_parser, &tn5250::client::Decoder::controlCharactersReceived, this, &MainWindow::onControlCharactersReceived);
    connect(m_parser, &tn5250::client::Decoder::sohReceived, this, &MainWindow::onSohReceived);
    connect(m_parser, &tn5250::client::Decoder::rollRequested, this, &MainWindow::onRollRequested);
    connect(m_parser, &tn5250::client::Decoder::writeErrorCodeRequested, this, &MainWindow::onWriteErrorCode);
    connect(m_parser, &tn5250::client::Decoder::saveScreenRequested, this, &MainWindow::onSaveScreenRequested);
    connect(m_parser, &tn5250::client::Decoder::clearScreenAlternateRequested, this, &MainWindow::onClearScreenAlternateRequested);
    connect(m_parser, &tn5250::client::Decoder::clearFormatTableRequested, this, &MainWindow::onClearFormatTableRequested);
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

void MainWindow::onClearScreenRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    m_displayWidget->screenBuffer()->clear();
    m_displayWidget->screenBuffer()->notifyCursor();
}

void MainWindow::onKeyboardUnlockRequested() {
    // The WTD command signaled that the keyboard should be unlocked.
    // This means the host has finished sending the screen and the user can type.
    logger::Logger::instance()->debug("MainWindow: Keyboard unlock requested by host");
    if (m_displayWidget) {
        m_displayWidget->setFocus();
    }
}

void MainWindow::onControlCharactersReceived(uint8_t cc1, uint8_t cc2) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    auto *screen = m_displayWidget->screenBuffer();

    // CC byte 1: bits 0-2 control MDT and field clearing
    // Per IBM SA21-9247-6 "Control Characters, Display"
    uint8_t combo = cc1 & 0x07;

    if (combo == 0x05 || combo == 0x06 || combo == 0x07) {
        // Null all non-bypass input fields
        for (const auto &field : screen->fields()) {
            if (!field.protected_field) {
                int addr = field.startRow * screen->cols() + field.startCol;
                for (int j = 0; j < field.length; ++j) {
                    int r = (addr + j) / screen->cols();
                    int c = (addr + j) % screen->cols();
                    screen->writeChar(r, c, 0x00);
                }
            }
        }
    }

    if (combo & 0x04) {
        // Bit 2: Reset MDT flags on all fields
        QVector<ui::widgets::ScreenBuffer::Field> &fields =
            const_cast<QVector<ui::widgets::ScreenBuffer::Field> &>(screen->fields());
        for (auto &field : fields) {
            field.modified = false;
        }
    }

    if (combo & 0x01) {
        // Bit 0: Reset pending aid, lock keyboard
        m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Locked);
    }

    if (combo == 0x07) {
        // Clear format table as well
        const_cast<QVector<ui::widgets::ScreenBuffer::Field> &>(screen->fields()).clear();
    }

    // CC byte 2 processing
    if (cc2 & 0x04) {
        // Bit 2: Reset blinking cursor
        m_displayWidget->setCursorBlinkRate(0);
    }
    if (cc2 & 0x08) {
        // Bit 3: Set blinking cursor
        m_displayWidget->setCursorBlinkRate(250);
    }
    if (cc2 & 0x10) {
        // Bit 4: Unlock keyboard, move cursor to IC address
        m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Unlocked);
        int icRow = m_displayWidget->icRow();
        int icCol = m_displayWidget->icCol();
        screen->setCursorPosition(icRow, icCol);
        // If IC address lands on a protected/non-field position (e.g., no IC order
        // was sent so default 0,0 is used), advance to the first input field.
        auto field = screen->getField(icRow, icCol);
        if (field.length <= 0 || field.protected_field || field.bypass) {
            const auto &fields = screen->fields();
            for (const auto &f : fields) {
                if (f.length > 0 && !f.protected_field && !f.bypass) {
                    screen->setCursorPosition(f.startRow, f.startCol);
                    break;
                }
            }
        }
        m_displayWidget->setFocus();
    }
    if (cc2 & 0x20) {
        // Bit 5: Sound alarm
        QApplication::beep();
    }
    if (cc2 & 0x40) {
        // Bit 6: Set Message Waiting indicator OFF
        m_displayWidget->setMessageWaiting(false);
    }
    if (cc2 & 0x80) {
        // Bit 7: Set Message Waiting indicator ON
        m_displayWidget->setMessageWaiting(true);
    }
}

void MainWindow::onSohReceived(uint8_t errorRow, uint8_t ckm1, uint8_t ckm2, uint8_t ckm3) {
    if (!m_displayWidget) {
        return;
    }
    // Store SOH fields in the terminal state
    if (errorRow > 0) {
        m_displayWidget->setErrorLineRow(errorRow - 1); // Convert to 0-based
    }
    m_displayWidget->setCmdKeyMask(ckm1, ckm2, ckm3);
    logger::Logger::instance()->debug(
        QString("MainWindow: SOH received - errorRow=%1 cmdKeyMask=%2,%3,%4")
            .arg(errorRow).arg(ckm1, 2, 16, QChar('0'))
            .arg(ckm2, 2, 16, QChar('0')).arg(ckm3, 2, 16, QChar('0')));
}

void MainWindow::onRollRequested(uint8_t topRow, uint8_t botRow, uint8_t lines, bool up) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    m_displayWidget->screenBuffer()->scrollRegion(topRow, botRow, lines, up);
    logger::Logger::instance()->debug(
        QString("MainWindow: Roll %1 rows=%2-%3 lines=%4")
            .arg(up ? "up" : "down").arg(topRow).arg(botRow).arg(lines));
}

void MainWindow::onWriteErrorCode(const QByteArray &errorData) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    auto *screen = m_displayWidget->screenBuffer();
    int errRow = m_displayWidget->errorLineRow();
    if (errRow < 0) errRow = screen->rows() - 1;

    // Save error line contents for Error Reset
    QVector<ui::widgets::ScreenCell> savedLine;
    for (int c = 0; c < screen->cols(); ++c) {
        savedLine.append(screen->cell(errRow, c));
    }
    m_displayWidget->setSavedErrorLine(savedLine);

    // Clear the error line first
    for (int c = 0; c < screen->cols(); ++c) {
        screen->writeChar(errRow, c, 0x40); // EBCDIC space
    }

    // Write error data to error line — contains orders/data for the error line
    int col = 0;
    ui::widgets::CellAttributes errAttr;
    errAttr.color = 12; // Red/high-intensity for error messages
    for (int i = 0; i < errorData.size();) {
        uint8_t byte = static_cast<uint8_t>(errorData[i]);
        if (byte == 0x13 && i + 2 < errorData.size()) {
            // IC order within WEC: move cursor (but don't change system IC address)
            int icRow = static_cast<uint8_t>(errorData[i + 1]) - 1;
            int icCol = static_cast<uint8_t>(errorData[i + 2]) - 1;
            if (icRow >= 0 && icRow < screen->rows() && icCol >= 0 && icCol < screen->cols()) {
                screen->setCursorPosition(icRow, icCol);
            }
            i += 3;
            continue;
        }
        if (byte >= 0x40 && col < screen->cols()) {
            // EBCDIC printable character — write to error line
            screen->writeChar(errRow, col, byte, errAttr);
            col++;
        } else if (byte >= 0x20 && byte <= 0x3F) {
            // Attribute byte — occupies a position as blank
            screen->writeChar(errRow, col, 0x40, errAttr);
            col++;
        }
        i++;
    }

    // Lock keyboard in ErrorLocked state
    m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::ErrorLocked);
    logger::Logger::instance()->debug("MainWindow: Write Error Code received");
}

void MainWindow::onSaveScreenRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        m_sessions[m_activeIndex]->savedScreen = m_displayWidget->screenBuffer()->saveState();
        logger::Logger::instance()->debug("MainWindow: Screen saved");
    }
}

void MainWindow::onClearScreenAlternateRequested() {
    if (!m_displayWidget) {
        return;
    }
    // Switch to 27x132 mode and clear
    m_displayWidget->setScreenSize(27, 132);
    if (m_displayWidget->screenBuffer()) {
        m_displayWidget->screenBuffer()->clear();
    }
    logger::Logger::instance()->debug("MainWindow: Clear Unit Alternate (27x132)");
}

void MainWindow::onClearFormatTableRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    m_displayWidget->screenBuffer()->clearFields();
    logger::Logger::instance()->debug("MainWindow: Format table cleared");
}

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

/**
 * Populate a QComboBox with the list of saved sessions.
 *
 * @param combo        Target combo box to fill.
 * @param placeholder  First item text shown when no session is chosen.
 */
void MainWindow::renderTN5250Stream(const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    auto *screen = m_displayWidget->screenBuffer();
    int currentRow = 0;
    int currentCol = 0;
    ui::widgets::CellAttributes currentAttr;
    int i = 0;
    int totalCells = screen->rows() * screen->cols();

    while (i < data.size()) {
        uint8_t byte = static_cast<uint8_t>(data[i]);

        // 5250 display orders are in the 0x00-0x3F range.
        // EBCDIC printable characters are 0x40-0xFF.
        if (byte >= 0x40) {
            // Regular EBCDIC character data — write to screen
            screen->writeChar(currentRow, currentCol, byte, currentAttr);
            currentCol++;
            if (currentCol >= screen->cols()) {
                currentCol = 0;
                currentRow++;
                if (currentRow >= screen->rows()) {
                    currentRow = screen->rows() - 1;
                }
            }
            i++;
            continue;
        }

        // Handle display orders (0x00-0x3F range)
        switch (byte) {
        case 0x11: { // SBA - Set Buffer Address
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            currentRow = static_cast<uint8_t>(data[i + 1]) - 1;
            currentCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (currentRow < 0) currentRow = 0;
            if (currentCol < 0) currentCol = 0;
            if (currentRow >= screen->rows()) currentRow = screen->rows() - 1;
            if (currentCol >= screen->cols()) currentCol = screen->cols() - 1;
            // Reset current attributes to default (green, visible).
            // In the 5250 protocol, field attributes apply only within the field.
            // Text outside any field uses default attributes. SBA starts a new
            // positioning context; if the next bytes are field content, the SF
            // order preceding them will re-set currentAttr appropriately.
            currentAttr = ui::widgets::CellAttributes();
            i += 3;
            break;
        }

        case 0x1D: { // SF - Start Field
            // Format: 0x1D FFW1 FFW2 [FCW pairs...] attr fieldLen
            if (i + 4 >= data.size()) {
                i = data.size();
                break;
            }
            uint8_t ffw1 = static_cast<uint8_t>(data[i + 1]);
            uint8_t ffw2 = static_cast<uint8_t>(data[i + 2]);
            int idx = i + 3; // past 0x1D, FFW1, FFW2

            // Skip optional FCW (Field Control Word) pairs.
            // FCW bytes have high 3 bits != 001. The attribute byte has bits 7-5 = 001 (0x20-0x3F).
            while (idx + 1 < data.size()) {
                uint8_t b = static_cast<uint8_t>(data[idx]);
                if ((b & 0xE0) == 0x20) {
                    break; // Found attribute byte
                }
                idx += 2; // Skip FCW pair
            }

            if (idx >= data.size()) { i = data.size(); break; }
            uint8_t attrByte = static_cast<uint8_t>(data[idx]);
            idx++;

            if (idx + 1 >= data.size()) { i = data.size(); break; }
            int fieldLen = (static_cast<uint8_t>(data[idx]) << 8) | static_cast<uint8_t>(data[idx + 1]);
            idx += 2;

            // Write BLANK at attribute byte position (it occupies a cell but is not displayed)
            ui::widgets::CellAttributes attrPosAttr;
            attrPosAttr.protected_field = true;
            attrPosAttr.nonDisplay = true;
            screen->writeChar(currentRow, currentCol, 0x40, attrPosAttr);

            // Advance past attribute byte position
            int nextAddr = currentRow * screen->cols() + currentCol + 1;
            int fieldStartRow = nextAddr / screen->cols();
            int fieldStartCol = nextAddr % screen->cols();

            // Register the field
            bool isProtected = (ffw1 & 0x20) != 0;
            screen->setField(fieldStartRow, fieldStartCol, fieldLen, isProtected);
            screen->setFieldFFW(fieldStartRow, fieldStartCol, ffw1, ffw2);

            // IBM 3179-2 full color attribute table (SA21-9247-6, Table 2-3)
            // Attribute byte format: 001X XXXX (0x20-0x3F), indexed by (attrByte & 0x1F)
            //
            // Each entry: { colorIndex, reverse, blink, underline, nonDisplay, colSep }
            // Color indices: 2=Green, 9=Blue, 11=Cyan, 12=Red, 13=Pink, 14=Yellow, 15=White
            struct AttrEntry { uint8_t color; bool rev; bool blk; bool ul; bool nd; bool cs; };
            static const AttrEntry attrTable[32] = {
                /* 0x20 */ { 2, false,false,false,false,false}, // Green
                /* 0x21 */ { 2,  true,false,false,false,false}, // Green, reverse
                /* 0x22 */ {15, false,false,false,false,false}, // White
                /* 0x23 */ {15,  true,false,false,false,false}, // White, reverse
                /* 0x24 */ { 2, false,false, true,false,false}, // Green, underline
                /* 0x25 */ { 2,  true,false, true,false,false}, // Green, reverse+underline
                /* 0x26 */ {15, false,false, true,false,false}, // White, underline
                /* 0x27 */ { 2, false,false,false, true,false}, // Non-display
                /* 0x28 */ {12, false,false,false,false,false}, // Red
                /* 0x29 */ {12,  true,false,false,false,false}, // Red, reverse
                /* 0x2A */ {12, false, true,false,false,false}, // Red, blink
                /* 0x2B */ {12,  true, true,false,false,false}, // Red, reverse+blink
                /* 0x2C */ {12, false,false, true,false,false}, // Red, underline
                /* 0x2D */ {12,  true,false, true,false,false}, // Red, reverse+underline
                /* 0x2E */ {12, false, true, true,false,false}, // Red, blink+underline
                /* 0x2F */ {12, false,false,false, true,false}, // Non-display
                /* 0x30 */ {11, false,false,false,false, true}, // Cyan, col-sep
                /* 0x31 */ {11,  true,false,false,false, true}, // Cyan, reverse, col-sep
                /* 0x32 */ {14, false,false,false,false, true}, // Yellow, col-sep
                /* 0x33 */ {14,  true,false,false,false, true}, // Yellow, reverse, col-sep
                /* 0x34 */ {11, false,false, true,false, true}, // Cyan, underline, col-sep
                /* 0x35 */ {11,  true,false, true,false, true}, // Cyan, reverse+underline, col-sep
                /* 0x36 */ {14, false,false, true,false, true}, // Yellow, underline, col-sep
                /* 0x37 */ {14, false,false,false, true,false}, // Non-display
                /* 0x38 */ {13, false,false,false,false,false}, // Pink
                /* 0x39 */ {13,  true,false,false,false,false}, // Pink, reverse
                /* 0x3A */ { 9, false,false,false,false,false}, // Blue
                /* 0x3B */ { 9,  true,false,false,false,false}, // Blue, reverse
                /* 0x3C */ {13, false,false, true,false,false}, // Pink, underline
                /* 0x3D */ {13,  true,false, true,false,false}, // Pink, reverse+underline
                /* 0x3E */ { 9, false,false, true,false,false}, // Blue, underline
                /* 0x3F */ { 9, false,false,false, true,false}, // Non-display
            };

            currentAttr = ui::widgets::CellAttributes();
            currentAttr.protected_field = isProtected;

            uint8_t tableIdx = attrByte & 0x1F;
            const AttrEntry &ae = attrTable[tableIdx];
            currentAttr.color = ae.color;
            currentAttr.reverse = ae.rev;
            currentAttr.blink = ae.blk;
            currentAttr.underline = ae.ul;
            currentAttr.nonDisplay = ae.nd;
            currentAttr.colSep = ae.cs;

            currentRow = fieldStartRow;
            currentCol = fieldStartCol;
            i = idx;
            break;
        }

        case 0x02: { // RA - Repeat to Address
            if (i + 3 >= data.size()) {
                i = data.size();
                break;
            }
            int targetRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int targetCol = static_cast<uint8_t>(data[i + 2]) - 1;
            uint8_t fillChar = static_cast<uint8_t>(data[i + 3]);
            if (targetRow < 0) targetRow = 0;
            if (targetCol < 0) targetCol = 0;
            i += 4;

            int currentAddr = currentRow * screen->cols() + currentCol;
            int targetAddr = targetRow * screen->cols() + targetCol;

            for (int addr = currentAddr; addr <= targetAddr && addr < totalCells; ++addr) {
                int r = addr / screen->cols();
                int c = addr % screen->cols();
                screen->writeChar(r, c, fillChar, currentAttr);
            }

            // Advance cursor past the last filled position
            int nextAddr = targetAddr + 1;
            if (nextAddr < totalCells) {
                currentRow = nextAddr / screen->cols();
                currentCol = nextAddr % screen->cols();
            } else {
                currentRow = screen->rows() - 1;
                currentCol = screen->cols() - 1;
            }
            break;
        }

        case 0x03: { // EA - Erase to Address
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int targetRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int targetCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (targetRow < 0) targetRow = 0;
            if (targetCol < 0) targetCol = 0;
            i += 3;

            int currentAddr = currentRow * screen->cols() + currentCol;
            int targetAddr = targetRow * screen->cols() + targetCol;

            for (int addr = currentAddr; addr <= targetAddr && addr < totalCells; ++addr) {
                int r = addr / screen->cols();
                int c = addr % screen->cols();
                screen->writeChar(r, c, 0x40, ui::widgets::CellAttributes());
            }

            int nextAddr = targetAddr + 1;
            if (nextAddr < totalCells) {
                currentRow = nextAddr / screen->cols();
                currentCol = nextAddr % screen->cols();
            } else {
                currentRow = screen->rows() - 1;
                currentCol = screen->cols() - 1;
            }
            break;
        }

        case 0x13: { // IC - Insert Cursor
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int icRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int icCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (icRow < 0) icRow = 0;
            if (icCol < 0) icCol = 0;
            screen->setCursorPosition(icRow, icCol);
            // Store IC address for CC "unlock + move to IC" processing
            if (m_displayWidget) {
                m_displayWidget->setICAddress(icRow, icCol);
            }
            i += 3;
            break;
        }

        case 0x14: { // MC - Move Cursor
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int mcRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int mcCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (mcRow < 0) mcRow = 0;
            if (mcCol < 0) mcCol = 0;
            currentRow = mcRow;
            currentCol = mcCol;
            i += 3;
            break;
        }

        case 0x15: { // WDSF - Write to Display Structured Field
            // Format: 0x15 [lenHi lenLo] [type] [data...]
            // Length includes the 2 length bytes themselves.
            // Skip entirely — advanced feature not yet implemented.
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int wdsfLen = (static_cast<uint8_t>(data[i + 1]) << 8) |
                           static_cast<uint8_t>(data[i + 2]);
            if (wdsfLen < 2) wdsfLen = 2;
            i += 1 + wdsfLen; // order byte + structured field
            if (i > data.size()) i = data.size();
            break;
        }

        case 0x10: { // TD - Transparent Data
            // Format: 0x10 [count_hi count_lo] [count bytes of transparent data]
            // Transparent data bytes are written to the screen as-is without
            // interpreting them as display orders.
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            uint16_t tdLen = (static_cast<uint16_t>(static_cast<uint8_t>(data[i + 1])) << 8) |
                              static_cast<uint16_t>(static_cast<uint8_t>(data[i + 2]));
            i += 3;
            for (uint16_t t = 0; t < tdLen && i < data.size(); t++) {
                uint8_t tdByte = static_cast<uint8_t>(data[i]);
                screen->writeChar(currentRow, currentCol, tdByte, currentAttr);
                currentCol++;
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        currentRow = screen->rows() - 1;
                    }
                }
                i++;
            }
            break;
        }

        case 0x04: { // ESC - should not appear here (already stripped by Decoder)
            i += 2;
            break;
        }

        default: {
            if (byte == 0x00) {
                // Null character — write to screen (marks unused field positions)
                screen->writeChar(currentRow, currentCol, 0x00, currentAttr);
                currentCol++;
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        currentRow = screen->rows() - 1;
                    }
                }
                i++;
                break;
            }
            if (byte >= 0x20 && byte <= 0x3F) {
                // Attribute indicator byte — occupies a display position (shown as blank)
                // and sets display attributes for subsequent characters.
                struct AttrEntry { uint8_t color; bool rev; bool blk; bool ul; bool nd; bool cs; };
                static const AttrEntry attrTable[32] = {
                    /* 0x20 */ { 2, false,false,false,false,false},
                    /* 0x21 */ { 2,  true,false,false,false,false},
                    /* 0x22 */ {15, false,false,false,false,false},
                    /* 0x23 */ {15,  true,false,false,false,false},
                    /* 0x24 */ { 2, false,false, true,false,false},
                    /* 0x25 */ { 2,  true,false, true,false,false},
                    /* 0x26 */ {15, false,false, true,false,false},
                    /* 0x27 */ { 2, false,false,false, true,false},
                    /* 0x28 */ {12, false,false,false,false,false},
                    /* 0x29 */ {12,  true,false,false,false,false},
                    /* 0x2A */ {12, false, true,false,false,false},
                    /* 0x2B */ {12,  true, true,false,false,false},
                    /* 0x2C */ {12, false,false, true,false,false},
                    /* 0x2D */ {12,  true,false, true,false,false},
                    /* 0x2E */ {12, false, true, true,false,false},
                    /* 0x2F */ {12, false,false,false, true,false},
                    /* 0x30 */ {11, false,false,false,false, true},
                    /* 0x31 */ {11,  true,false,false,false, true},
                    /* 0x32 */ {14, false,false,false,false, true},
                    /* 0x33 */ {14,  true,false,false,false, true},
                    /* 0x34 */ {11, false,false, true,false, true},
                    /* 0x35 */ {11,  true,false, true,false, true},
                    /* 0x36 */ {14, false,false, true,false, true},
                    /* 0x37 */ {14, false,false,false, true,false},
                    /* 0x38 */ {13, false,false,false,false,false},
                    /* 0x39 */ {13,  true,false,false,false,false},
                    /* 0x3A */ { 9, false,false,false,false,false},
                    /* 0x3B */ { 9,  true,false,false,false,false},
                    /* 0x3C */ {13, false,false, true,false,false},
                    /* 0x3D */ {13,  true,false, true,false,false},
                    /* 0x3E */ { 9, false,false, true,false,false},
                    /* 0x3F */ { 9, false,false,false, true,false},
                };
                // Write blank at attribute position (not displayed)
                ui::widgets::CellAttributes attrPosAttr;
                attrPosAttr.protected_field = true;
                attrPosAttr.nonDisplay = true;
                screen->writeChar(currentRow, currentCol, 0x40, attrPosAttr);
                // Update current attributes from the attribute table
                uint8_t tableIdx = byte & 0x1F;
                const AttrEntry &ae = attrTable[tableIdx];
                currentAttr = ui::widgets::CellAttributes();
                currentAttr.color = ae.color;
                currentAttr.reverse = ae.rev;
                currentAttr.blink = ae.blk;
                currentAttr.underline = ae.ul;
                currentAttr.nonDisplay = ae.nd;
                currentAttr.colSep = ae.cs;
                currentCol++;
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        currentRow = screen->rows() - 1;
                    }
                }
                i++;
                break;
            }
            // Truly unrecognized control byte — skip silently
            i++;
            break;
        }
        }
    }

    // Notify screen changed
    m_displayWidget->update();
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

QByteArray MainWindow::buildFieldResponse(uint8_t aidByte) {
    QByteArray response;
    response.append(static_cast<char>(aidByte));

    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return response;
    }

    auto *screen = m_displayWidget->screenBuffer();
    QPoint cursor = screen->cursorPosition();
    // Cursor position: 1-based row and col
    response.append(static_cast<char>(cursor.y() + 1));
    response.append(static_cast<char>(cursor.x() + 1));

    // Append modified fields: SBA(0x11) + row(1-based) + col(1-based) + field data
    QVector<ui::widgets::ScreenBuffer::Field> modFields = screen->getModifiedFields();
    for (const auto &field : modFields) {
        response.append(static_cast<char>(0x11)); // SBA order
        response.append(static_cast<char>(field.startRow + 1));
        response.append(static_cast<char>(field.startCol + 1));
        response.append(screen->getFieldData(field));
    }

    return response;
}

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