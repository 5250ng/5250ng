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

    ui::widgets::ScreenBuffer *screen = m_displayWidget->screenBuffer();
    auto hostRowToIndex = [](uint8_t hostRow) -> int {
        // 5250 row/col addressing is typically 1-based.
        // Internally we use 0-based coordinates.
        return hostRow > 0 ? static_cast<int>(hostRow) - 1 : 0;
    };
    auto hostColToIndex = [](uint8_t hostCol) -> int {
        return hostCol > 0 ? static_cast<int>(hostCol) - 1 : 0;
    };
    auto applyAttrByte = [](ui::widgets::CellAttributes &attr, uint8_t attrByte) {
        // Attribute encoding used by this emulator:
        // Bits 0-3: Color (0-15)
        // Bit 4: Reverse video
        // Bit 5: Blink
        // Bit 6: Underline
        // Bit 7: Protected field
        attr.color = attrByte & 0x0F;
        attr.reverse = (attrByte & 0x10) != 0;
        attr.blink = (attrByte & 0x20) != 0;
        attr.underline = (attrByte & 0x40) != 0;
        attr.protected_field = (attrByte & 0x80) != 0;
    };
    logger::Logger::instance()->debug(
        QString("MainWindow: Handling TN5250 command: %1, data size: %2")
            .arg(static_cast<int>(cmd))
            .arg(data.size())
    );

    switch (cmd) {
    case tn5250::client::TN5250Command::ERASE_WRITE: {
        // Format: [row(1)] [col(1)] [data...]
        // Data may contain Start Field (SF) markers (0x11) followed by attribute
        // bytes
        if (data.size() >= 2) {
            uint8_t hostRow = static_cast<uint8_t>(data[0]);
            uint8_t hostCol = static_cast<uint8_t>(data[1]);
            int row = hostRowToIndex(hostRow);
            int col = hostColToIndex(hostCol);
            QByteArray screenData = data.mid(2);

            logger::Logger::instance()->debug(
                QString("MainWindow: ERASE_WRITE at row=%1, col=%2, data=%3 bytes")
                    .arg(hostRow)
                    .arg(hostCol)
                    .arg(screenData.size())
            );

            // Erase from cursor position
            screen->eraseWrite(row, col, screenData.size());

            // Write data, parsing Start Field (SF) markers for attributes
            ui::widgets::CellAttributes attr;
            int currentRow = row;
            int currentCol = col;
            for (int i = 0; i < screenData.size(); i++) {
                uint8_t ch = static_cast<uint8_t>(screenData[i]);

                // Check for Start Field (SF) marker (commonly 0x11 in this emulator;
                // some 5250 streams use 0x1D)
                if ((ch == 0x11 || ch == 0x1D) && i + 1 < screenData.size()) {
                    uint8_t attrByte = static_cast<uint8_t>(screenData[i + 1]);

                    applyAttrByte(attr, attrByte);

                    logger::Logger::instance()->debug(
                        QString("MainWindow: ERASE_WRITE: Start Field (SF) at offset %1, "
                                "attribute byte=0x%2, color=%3, reverse=%4, blink=%5, "
                                "underline=%6, protected=%7")
                            .arg(i)
                            .arg(attrByte, 2, 16, QChar('0'))
                            .arg(attr.color)
                            .arg(attr.reverse)
                            .arg(attr.blink)
                            .arg(attr.underline)
                            .arg(attr.protected_field)
                    );

                    // Skip both the SF marker (0x11) and the attribute byte
                    i++;      // Skip attribute byte
                    continue; // Don't write these bytes to screen
                }

                // Handle line wrapping
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        // Screen is full, scroll up
                        screen->scrollUp(1);
                        currentRow = screen->rows() - 1;
                    }
                }

                // Write the character with current attributes
                screen->writeChar(currentRow, currentCol, ch, attr);
                currentCol++;
            }

            // Update cursor position
            screen->setCursorPosition(currentRow, currentCol);
        }
        break;
    }

    case tn5250::client::TN5250Command::ERASE_WRITE_ALTERNATE: {
        // Similar to ERASE_WRITE but with alternate format
        // Data may contain Start Field (SF) markers (0x11) followed by attribute
        // bytes
        if (data.size() >= 2) {
            uint8_t hostRow = static_cast<uint8_t>(data[0]);
            uint8_t hostCol = static_cast<uint8_t>(data[1]);
            int row = hostRowToIndex(hostRow);
            int col = hostColToIndex(hostCol);
            QByteArray screenData = data.mid(2);

            logger::Logger::instance()->debug(
                QString("MainWindow: ERASE_WRITE_ALTERNATE at row=%1, col=%2, "
                        "data=%3 bytes")
                    .arg(hostRow)
                    .arg(hostCol)
                    .arg(screenData.size())
            );

            screen->eraseWriteAlternate(row, col, screenData.size());

            // Write data, parsing Start Field (SF) markers for attributes
            ui::widgets::CellAttributes attr;
            int currentRow = row;
            int currentCol = col;
            for (int i = 0; i < screenData.size(); i++) {
                uint8_t ch = static_cast<uint8_t>(screenData[i]);

                // Check for Start Field (SF) marker (commonly 0x11 in this emulator;
                // some 5250 streams use 0x1D)
                if ((ch == 0x11 || ch == 0x1D) && i + 1 < screenData.size()) {
                    uint8_t attrByte = static_cast<uint8_t>(screenData[i + 1]);

                    applyAttrByte(attr, attrByte);

                    logger::Logger::instance()->debug(
                        QString("MainWindow: ERASE_WRITE_ALTERNATE: Start Field (SF) at "
                                "offset %1, attribute byte=0x%2, color=%3, reverse=%4, "
                                "blink=%5, underline=%6, protected=%7")
                            .arg(i)
                            .arg(attrByte, 2, 16, QChar('0'))
                            .arg(attr.color)
                            .arg(attr.reverse)
                            .arg(attr.blink)
                            .arg(attr.underline)
                            .arg(attr.protected_field)
                    );

                    // Skip both the SF marker (0x11) and the attribute byte
                    i++;      // Skip attribute byte
                    continue; // Don't write these bytes to screen
                }

                // Handle line wrapping
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        // Screen is full, scroll up
                        screen->scrollUp(1);
                        currentRow = screen->rows() - 1;
                    }
                }

                // Write the character with current attributes
                screen->writeChar(currentRow, currentCol, ch, attr);
                currentCol++;
            }

            screen->setCursorPosition(currentRow, currentCol);
        }
        break;
    }

    case tn5250::client::TN5250Command::READ_MODIFY:
    case tn5250::client::TN5250Command::READ_MODIFY_WRITE: {
        // These commands are for input fields - handle later
        logger::Logger::instance()->debug(
            QString("MainWindow: READ_MODIFY command (not yet implemented)")
        );
        break;
    }

    case tn5250::client::TN5250Command::WRITE_STRUCTURED_FIELD: {
        // Structured fields are handled separately
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

    ui::widgets::ScreenBuffer *screen = m_displayWidget->screenBuffer();
    auto hostRowToIndex = [](uint8_t hostRow) -> int {
        return hostRow > 0 ? static_cast<int>(hostRow) - 1 : 0;
    };
    auto hostColToIndex = [](uint8_t hostCol) -> int {
        return hostCol > 0 ? static_cast<int>(hostCol) - 1 : 0;
    };
    auto applyAttrByte = [](ui::widgets::CellAttributes &attr, uint8_t attrByte) {
        attr.color = attrByte & 0x0F;
        attr.reverse = (attrByte & 0x10) != 0;
        attr.blink = (attrByte & 0x20) != 0;
        attr.underline = (attrByte & 0x40) != 0;
        attr.protected_field = (attrByte & 0x80) != 0;
    };
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
            uint8_t flags = static_cast<uint8_t>(data[0]);
            uint8_t hostRow = static_cast<uint8_t>(data[1]);
            uint8_t hostCol = static_cast<uint8_t>(data[2]);
            int row = hostRowToIndex(hostRow);
            int col = hostColToIndex(hostCol);
            QByteArray screenData = data.mid(3);

            logger::Logger::instance()->debug(
                QString("MainWindow: OUTBOUND_5250_DS at row=%1, col=%2, data=%3 "
                        "bytes, flags=0x%4")
                    .arg(hostRow)
                    .arg(hostCol)
                    .arg(screenData.size())
                    .arg(flags, 2, 16, QChar('0'))
            );

            ui::widgets::CellAttributes attr;
            int currentRow = row;
            int currentCol = col;
            QPoint cursorTarget(-1, -1); // x=col, y=row; -1 => not set

            // Parse the 5250 data stream payload.
            // This emulator understands:
            // - SBA 0x11 + Row + Col (positions write cursor; 1-based)
            // - SF 0x1D + 1 byte attribute (simplified handling)
            // - MC order 0x14 + row + col (cursor target, 1-based per RFC1205)
            // - IC order 0x13 (cursor at current write position)
            for (int i = 0; i < screenData.size(); i++) {
                uint8_t ch = static_cast<uint8_t>(screenData[i]);

                // Set Buffer Address (SBA): 0x11 Row Col (1-based)
                if (ch == 0x11 && i + 2 < screenData.size()) {
                    uint8_t sbaRow = static_cast<uint8_t>(screenData[i + 1]);
                    uint8_t sbaCol = static_cast<uint8_t>(screenData[i + 2]);
                    currentRow = hostRowToIndex(sbaRow);
                    currentCol = hostColToIndex(sbaCol);
                    i += 2;
                    continue;
                }

                // Start Field marker (simplified): 0x1D + attribute byte
                if (ch == 0x1D && i + 1 < screenData.size()) {
                    uint8_t attrByte = static_cast<uint8_t>(screenData[i + 1]);
                    applyAttrByte(attr, attrByte);
                    i++;
                    continue;
                }

                // Move Cursor order (RFC1205 5.3): X'14' Row Column (1-based)
                if (ch == 0x14 && i + 2 < screenData.size()) {
                    uint8_t mcRow = static_cast<uint8_t>(screenData[i + 1]);
                    uint8_t mcCol = static_cast<uint8_t>(screenData[i + 2]);
                    cursorTarget = QPoint(hostColToIndex(mcCol), hostRowToIndex(mcRow));
                    i += 2;
                    continue;
                }

                // Insert Cursor order: cursor at current write position
                if (ch == 0x13) {
                    cursorTarget = QPoint(currentCol, currentRow);
                    continue;
                }

                // Handle line wrapping
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        screen->scrollUp(1);
                        currentRow = screen->rows() - 1;
                    }
                }

                screen->writeChar(currentRow, currentCol, ch, attr);
                currentCol++;
            }

            if (cursorTarget.x() >= 0 && cursorTarget.y() >= 0) {
                screen->setCursorPosition(cursorTarget.y(), cursorTarget.x());
            } else {
                screen->setCursorPosition(currentRow, currentCol);
            }
        }
        break;
    }

    case tn5250::client::StructuredFieldType::SCS: {
        // SCS (Screen Control Sequence) - handle cursor, attributes, etc.
        logger::Logger::instance()->debug("MainWindow: SCS structured field");
        // TODO: Implement SCS handling
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
 * Handle raw TN5250 screen data payloads (EBCDIC plus field markers).
 *
 * Performs a simple parse loop to write characters into the screen buffer,
 * respecting Start Field markers for attributes and line wrapping behavior.
 * Also logs a hex dump of the payload for debugging.
 *
 * @param data Raw data bytes to render into the screen buffer.
 */
void MainWindow::handleRawScreenData(const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    ui::widgets::ScreenBuffer *screen = m_displayWidget->screenBuffer();
    auto applyAttrByte = [](ui::widgets::CellAttributes &attr, uint8_t attrByte) {
        attr.color = attrByte & 0x0F;
        attr.reverse = (attrByte & 0x10) != 0;
        attr.blink = (attrByte & 0x20) != 0;
        attr.underline = (attrByte & 0x40) != 0;
        attr.protected_field = (attrByte & 0x80) != 0;
    };
    logger::Logger::instance()->debug(
        QString("MainWindow: Handling raw screen data - %1 bytes")
            .arg(data.size())
    );
    std::vector<uint8_t> dumpBuf(reinterpret_cast<const uint8_t *>(data.constData()), reinterpret_cast<const uint8_t *>(data.constData()) + data.size());
    std::vector<std::string> hexLines = utils::hex::hexdump(dumpBuf);
    for (const std::string &line : hexLines) {
        logger::Logger::instance()->debug(QString::fromStdString(line));
    }

    // Helper to render a TN5250 character stream (handles SBA 0x11, SF 0x1D attributes)
    auto renderTN5250Stream = [&](const QByteArray &payload) {
        QPoint cursorPos = screen->cursorPosition();
        int row = cursorPos.y();
        int col = cursorPos.x();
        ui::widgets::CellAttributes attr;

        for (int i = 0; i < payload.size(); i++) {
            uint8_t ch = static_cast<uint8_t>(payload[i]);

            // Set Buffer Address (SBA): 0x11 Row Col (1-based)
            if (ch == 0x11 && i + 2 < payload.size()) {
                uint8_t hostRow = static_cast<uint8_t>(payload[i + 1]);
                uint8_t hostCol = static_cast<uint8_t>(payload[i + 2]);
                row = hostRow > 0 ? static_cast<int>(hostRow) - 1 : 0;
                col = hostCol > 0 ? static_cast<int>(hostCol) - 1 : 0;
                i += 2;
                continue;
            }

            // TN5250 Start Field (SF) simplified control character (0x1D)
            if (ch == 0x1D && i + 1 < payload.size()) {
                uint8_t attrByte = static_cast<uint8_t>(payload[i + 1]);
                attr.color = attrByte & 0x0F;                  // Lower 4 bits
                attr.reverse = (attrByte & 0x10) != 0;         // Bit 4
                attr.blink = (attrByte & 0x20) != 0;           // Bit 5
                attr.underline = (attrByte & 0x40) != 0;       // Bit 6
                attr.protected_field = (attrByte & 0x80) != 0; // Bit 7

                logger::Logger::instance()->debug(
                    QString("MainWindow: SF(0x1D) at pos %1 attr=0x%2 color=%3 rev=%4 "
                            "blink=%5 ul=%6 prot=%7")
                        .arg(i)
                        .arg(attrByte, 2, 16, QChar('0'))
                        .arg(attr.color)
                        .arg(attr.reverse)
                        .arg(attr.blink)
                        .arg(attr.underline)
                        .arg(attr.protected_field)
                );
                i++;      // Skip attribute byte
                continue; // Do not write SF bytes
            }

            // Line wrapping
            if (col >= screen->cols()) {
                col = 0;
                row++;
                if (row >= screen->rows()) {
                    screen->scrollUp(1);
                    row = screen->rows() - 1;
                }
            }
            // EBCDIC data byte
            screen->writeChar(row, col, ch, attr);
            col++;
        }
        screen->setCursorPosition(row, col);
        logger::Logger::instance()->debug(
            QString("MainWindow: Rendered TN5250 stream, cursor at %1/%2")
                .arg(row)
                .arg(col)
        );
    };

    // Try to parse RFC1205 header (GDS 0x12A0) and act based on opcode
    if (data.size() >= 10) {
        uint8_t b0 = static_cast<uint8_t>(data[0]);
        uint8_t b1 = static_cast<uint8_t>(data[1]);
        uint16_t recLen = (static_cast<uint16_t>(b0) << 8) | static_cast<uint16_t>(b1);
        uint8_t r2 = static_cast<uint8_t>(data[2]);
        uint8_t r3 = static_cast<uint8_t>(data[3]);
        if (r2 == 0x12 && r3 == 0xA0) {
            // Fixed 6-byte header
            int varHdrStart = 6;
            uint8_t varLen = static_cast<uint8_t>(data[varHdrStart]);
            if (data.size() >= varHdrStart + 1 + varLen && varLen >= 4) {
                // Flags (2 bytes) + Opcode (1 byte)
                uint8_t flagsHi = static_cast<uint8_t>(data[varHdrStart + 1]);
                uint8_t flagsLo = static_cast<uint8_t>(data[varHdrStart + 2]);
                uint8_t opcode = static_cast<uint8_t>(data[varHdrStart + 3]);
                uint16_t flags = (static_cast<uint16_t>(flagsHi) << 8) | flagsLo;

                int payloadStart = varHdrStart + varLen;
                int payloadLen = static_cast<int>(recLen) - (6 + varLen);
                if (payloadLen < 0)
                    payloadLen = 0;
                QByteArray payload;
                if (payloadStart >= 0 && payloadStart + payloadLen <= data.size()) {
                    payload = data.mid(payloadStart, payloadLen);
                } else if (payloadStart < data.size()) {
                    payload = data.mid(payloadStart);
                }

                logger::Logger::instance()->debug(
                    QString("MainWindow: RFC1205 header detected len=%1 flags=0x%2 opcode=0x%3 payload=%4")
                        .arg(recLen)
                        .arg(flags, 4, 16, QChar('0'))
                        .arg(opcode, 2, 16, QChar('0'))
                        .arg(payload.size())
                );

                switch (opcode) {
                case 0x00: { // No Operation
                    logger::Logger::instance()->debug("TN5250: NOP (no operation)");
                    return;
                }
                case 0x01: { // Invite Operation
                    logger::Logger::instance()->debug("TN5250: INVITE operation - host expects input");
                    // Future: set an invited state to gate sending input
                    return;
                }
                case 0x02: { // Output Only
                    logger::Logger::instance()->debug("TN5250: OUTPUT ONLY - rendering payload");
                    renderTN5250Stream(payload);
                    return;
                }
                case 0x03: { // Put/Get Operation
                    logger::Logger::instance()->debug("TN5250: PUT/GET - rendering payload");
                    renderTN5250Stream(payload);
                    return;
                }
                case 0x04: { // Save Screen Operation
                    logger::Logger::instance()->debug("TN5250: SAVE SCREEN request");
                    // Future: capture screen snapshot if needed
                    return;
                }
                case 0x05: { // Restore Screen Operation
                    logger::Logger::instance()->debug("TN5250: RESTORE SCREEN - rendering payload");
                    renderTN5250Stream(payload);
                    return;
                }
                case 0x06: { // Read Immediate
                    logger::Logger::instance()->debug("TN5250: READ IMMEDIATE - host requests immediate input");
                    // Future: gather and send input immediately
                    return;
                }
                case 0x08: { // Read Screen
                    logger::Logger::instance()->debug("TN5250: READ SCREEN - host requests full screen");
                    // Future: send full screen buffer
                    return;
                }
                case 0x0A: { // Cancel Invite
                    logger::Logger::instance()->debug("TN5250: CANCEL INVITE - stop sending user data until re-invited");
                    return;
                }
                case 0x0B: { // Turn On Message Light
                    logger::Logger::instance()->debug("TN5250: MESSAGE LIGHT ON");
                    return;
                }
                case 0x0C: { // Turn Off Message Light
                    logger::Logger::instance()->debug("TN5250: MESSAGE LIGHT OFF");
                    return;
                }
                default: {
                    logger::Logger::instance()->warning(
                        QString("TN5250: Unknown opcode 0x%1 - rendering payload as data")
                            .arg(opcode, 2, 16, QChar('0'))
                    );
                    renderTN5250Stream(payload);
                    return;
                }
                }
            }
        }
    }

    // Fallback: treat entire buffer as a TN5250 character stream
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