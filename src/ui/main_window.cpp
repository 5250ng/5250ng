#include "main_window.h"
#include "core/ebcdic.h"
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
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTabBar>
#include <QTimer>
#include <QWidgetAction>
#include "ui/themes/terminal_theme_manager.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScreen>
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
    : ui::widgets::BaseFramelessWindow(parent), m_displayWidget(nullptr),
      m_parser(nullptr), m_cursorCoordinates(nullptr), m_connected(false) {
    setWindowTitle("5250ng");
    // DPI-aware initial size: ~70% of primary screen, clamped to reasonable bounds
    {
        QSize initial(1128, 836);
        if (auto *screen = QApplication::primaryScreen()) {
            QRect avail = screen->availableGeometry();
            initial = QSize(avail.width() * 7 / 10, avail.height() * 7 / 10);
        }
        resize(initial);
    }
    setMinimumSize(640, 480);
    setAcceptDrops(true);

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
    for (Session *s : m_sessions) {
        if (s && s->worker) {
            QMetaObject::invokeMethod(s->worker, "stop", Qt::QueuedConnection);
        }
        if (s && s->thread) {
            s->thread->quit();
            s->thread->wait(2000);
        }
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

    // Apply the configured EBCDIC code page
    core::EBCDIC::setCodePage(config.codePage());

    // Ensure a unique device name per host so the AS/400 doesn't reject the
    // session.  Collect device names already in use for the same host:port,
    // then append a numeric suffix if there is a collision.
    QString deviceName = config.deviceName();
    {
        QSet<QString> usedNames;
        for (const Session *s : m_sessions) {
            if (s->config.hostname() == config.hostname()
                && s->config.port() == config.port()) {
                usedNames.insert(s->config.deviceName().toUpper());
            }
        }
        if (usedNames.contains(deviceName.toUpper())) {
            QString base = deviceName;
            // IBM device names are max 10 chars; trim base to leave room for suffix
            if (base.size() > 7) base.truncate(7);
            for (int i = 1; i <= 99; ++i) {
                QString candidate = QString("%1%2").arg(base).arg(i);
                if (!usedNames.contains(candidate.toUpper())) {
                    deviceName = candidate;
                    break;
                }
            }
        }
        m_currentSession.setDeviceName(deviceName);
    }

    // Create a new session tab
    Session *session = new Session();
    session->container = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(session->container);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    // Terminal view: screen + hrule + footer screen(1 row)
    ui::widgets::Q5250TerminalView *terminalView = new ui::widgets::Q5250TerminalView(session->container);
    tabLayout->addWidget(terminalView);
    session->terminalView = terminalView;
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
    // DPI-aware footer height: based on font metrics rather than fixed pixels
    int footerMinH = qMax(22, QFontMetrics(footerWidget->font()).height() + 8);
    footerWidget->setMinimumHeight(footerMinH);
    footerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    session->statusBar = footerWidget;
    tabLayout->addWidget(footerWidget);
    // Ensure the display fills remaining space and footer stays at bottom
    tabLayout->setStretch(0, 1); // terminal view grows
    tabLayout->setStretch(1, 0); // footer
    session->container->setLayout(tabLayout);

    // CRT overlay covers the entire tab (screen + hrule + footer + status bar)
    session->crtOverlay = new ui::widgets::QCRTOverlayWidget(session->container);
    session->crtOverlay->setVisible(false);
    session->crtOverlay->raise();
    // Invalidate bloom cache when screen content changes
    connect(session->displayWidget->screenBuffer(), &ui::widgets::ScreenBuffer::screenChanged,
            session->crtOverlay, &ui::widgets::QCRTOverlayWidget::invalidateBloom);
    // Keep overlay sized to container
    session->container->installEventFilter(this);
    session->parser = new tn5250::client::Decoder(session->container);
    session->thread = new QThread(this);
    session->worker = new tn5250::session::Worker();
    session->worker->setConfig(m_currentSession);
    session->worker->moveToThread(session->thread);
    connect(session->thread, &QThread::started, session->worker, &tn5250::session::Worker::start);
    connect(session->thread, &QThread::finished, session->worker, &QObject::deleteLater);
    // Per-session state handling — always updates this session's status widget,
    // and only touches global menu actions when this is the active tab.
    connect(session->worker, &tn5250::session::Worker::stateChanged, this,
        [this, session](tn5250::client::TN5250Client::ConnectionState state) {
            if (session->connectionStatus) {
                session->connectionStatus->setState(state);
                switch (state) {
                case tn5250::client::TN5250Client::ConnectionState::Connected:
                    session->connectionStatus->setStatusText(
                        QString("Connected to %1:%2")
                            .arg(session->config.hostname())
                            .arg(session->config.port()));
                    break;
                case tn5250::client::TN5250Client::ConnectionState::Connecting:
                    session->connectionStatus->setStatusText("Connecting");
                    break;
                case tn5250::client::TN5250Client::ConnectionState::Negotiating:
                    session->connectionStatus->setStatusText("Waiting for system");
                    break;
                default:
                    session->connectionStatus->setStatusText("Not connected");
                    break;
                }
            }
            if (m_sessions.indexOf(session) == m_activeIndex) {
                bool connected = (state == tn5250::client::TN5250Client::ConnectionState::Connected
                               || state == tn5250::client::TN5250Client::ConnectionState::Negotiating);
                m_connected = connected;
                m_connectAction->setEnabled(!connected);
                m_disconnectAction->setEnabled(connected);
            }
        });
    connect(session->worker, &tn5250::session::Worker::errorOccurred, this,
        [this, session](const QString &error) {
            logger::Logger::instance()->error(QString("Connection error: %1").arg(error));
            if (m_sessions.indexOf(session) == m_activeIndex) {
                ui::widgets::StyledMessageBox::warning(this, "Connection Error", error);
            }
        });
    // App data: feed this session's parser directly
    connect(session->worker, &tn5250::session::Worker::appData, this, [this, session](const QByteArray &bytes) {
        if (session->parser) {
            session->parser->parseData(bytes);
        } }, Qt::QueuedConnection);
    session->config = m_currentSession;

    // Use session name for saved sessions, ip:port for CLI/unsaved
    QString tabDisplayName;
    if (!config.name().isEmpty()
        && config.name() != "Current Session"
        && config.name() != "Command Line Session") {
        tabDisplayName = config.name();
    } else {
        tabDisplayName = QString("%1:%2").arg(config.hostname()).arg(config.port());
    }

    int newIndex = m_tabWidget->addTab(session->container, tabDisplayName);
    // Close button on the right side of the tab
    {
        QPushButton *closeBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"), this);
        closeBtn->setFlat(true);
        // DPI-aware close button size
        int closeBtnSize = qMax(16, qRound(16 * devicePixelRatioF()));
        closeBtn->setFixedSize(closeBtnSize, closeBtnSize);
        closeBtn->setToolTip("Close");
        m_tabWidget->tabBar()->setTabButton(newIndex, QTabBar::RightSide, closeBtn);
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

    // Apply terminal theme from session config
    {
        QString themeId = config.terminalThemeId();
        auto &mgr = ui::themes::TerminalThemeManager::instance();
        if (!mgr.hasTheme(themeId))
            themeId = ui::themes::TerminalThemeManager::defaultThemeId();
        applyThemeToSession(session, themeId);
    }

    // Wire display input — per-session, routes directly to this session's worker
    connect(session->displayWidget, &ui::widgets::Q5250ScreenWidget::inputReady, this,
        [session](const QByteArray &data) {
            if (session->worker) {
                QMetaObject::invokeMethod(session->worker, "sendInput",
                    Qt::QueuedConnection, Q_ARG(QByteArray, data));
            }
        });
    // Attention key — telnet-level GDS(flags=0x40)
    connect(session->displayWidget, &ui::widgets::Q5250ScreenWidget::attentionRequested, this,
        [session]() {
            if (session->worker) {
                QMetaObject::invokeMethod(session->worker, "sendAttention",
                    Qt::QueuedConnection);
            }
        });
    // System Request — telnet-level GDS(flags=0x04)
    connect(session->displayWidget, &ui::widgets::Q5250ScreenWidget::systemRequestRequested, this,
        [session]() {
            if (session->worker) {
                QMetaObject::invokeMethod(session->worker, "sendSystemRequest",
                    Qt::QueuedConnection);
            }
        });
    if (session->displayWidget->screenBuffer()) {
        connect(session->displayWidget->screenBuffer(), &ui::widgets::ScreenBuffer::cursorMoved, this,
            [this, session]() {
                if (m_sessions.indexOf(session) != m_activeIndex) return;
                if (!session->displayWidget || !session->displayWidget->screenBuffer()
                    || !session->coordinatesLabel) return;
                updateCursorCoordinatesFont();
                QPoint pos = session->displayWidget->screenBuffer()->cursorPosition();
                session->coordinatesLabel->setText(
                    QString("%1/%2").arg(pos.y() + 1).arg(pos.x() + 1));
            });
    }

    // Set active pointers and connect signals for this session
    setActiveSession(newIndex);

    // Create command handler for this session
    session->commandHandler = new ui::rendering::TN5250CommandHandler(session->container);
    session->commandHandler->setDisplayWidget(session->displayWidget);
    session->commandHandler->setSendToHostCallback(
        [session](const QByteArray &data) {
            if (session->worker) {
                QMetaObject::invokeMethod(session->worker, "sendInput",
                    Qt::QueuedConnection, Q_ARG(QByteArray, data));
            }
        });
    session->commandHandler->setSendGDSCallback(
        [session](uint8_t flagsHi, uint8_t opcode, const QByteArray &payload) {
            if (session->worker) {
                QMetaObject::invokeMethod(session->worker, "sendGDS",
                    Qt::QueuedConnection,
                    Q_ARG(uint8_t, flagsHi),
                    Q_ARG(uint8_t, opcode),
                    Q_ARG(QByteArray, payload));
            }
        });
    session->commandHandler->connectDecoder(session->parser);
    // Wire save screen — per-session
    connect(session->parser, &tn5250::client::Decoder::saveScreenRequested, this,
        [session]() {
            if (session->displayWidget && session->displayWidget->screenBuffer()) {
                session->savedScreen = session->displayWidget->screenBuffer()->saveState();
            }
        });

    // Start the session thread (which triggers the worker start)
    session->thread->start();
    // Status indicator will be updated via worker stateChanged signal
    updateEmptyState();
}

void MainWindow::onToggleCursorRules() {
    if (m_displayWidget) {
        m_displayWidget->toggleCursorRules();
        m_cursorRulesAction->setChecked(m_displayWidget->showCursorRules());
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
        m_parser = nullptr;
        m_connected = false;
        m_connectAction->setEnabled(true);
        m_disconnectAction->setEnabled(false);
        m_reconnectAction->setEnabled(false);
        m_duplicateAction->setEnabled(false);
        return;
    }
    m_activeIndex = index;
    Session *s = m_sessions[index];
    m_displayWidget = s->displayWidget;
    m_parser = s->parser;
    m_cursorCoordinates = s->coordinatesLabel;
    m_currentSession = s->config;
    // Sync menu actions with this session's actual connection state
    auto state = s->connectionStatus->state();
    bool connected = (state == tn5250::client::TN5250Client::ConnectionState::Connected
                   || state == tn5250::client::TN5250Client::ConnectionState::Negotiating);
    m_connected = connected;
    m_connectAction->setEnabled(!connected);
    m_disconnectAction->setEnabled(connected);
    m_reconnectAction->setEnabled(true);
    m_duplicateAction->setEnabled(true);
    updateCursorCoordinatesFont();
    updateCursorCoordinates();
}

void MainWindow::applyThemeToSession(Session *session, const QString &themeId) {
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    if (!mgr.hasTheme(themeId)) return;

    auto theme = mgr.resolvedTheme(themeId);

    // Background image stored on session, painted in container's event filter
    if (theme.backgroundMode == ui::themes::TerminalTheme::Image) {
        if (!theme.backgroundImagePath.isEmpty()) {
            session->bgImage = QPixmap(theme.backgroundImagePath);
        } else if (!theme.backgroundImageData.isEmpty()) {
            session->bgImage.loadFromData(theme.backgroundImageData);
        } else {
            session->bgImage = QPixmap();
        }
        session->bgImageLayout = theme.backgroundImageLayout;
        session->bgImageOpacity = theme.backgroundImageOpacity;
    } else {
        session->bgImage = QPixmap();
    }
    // Invalidate cached scaled background so it's regenerated on next paint
    session->bgImageScaled = QPixmap();
    session->bgImageScaledSize = QSize();

    // Container transparency when background image is active
    bool hasBgImage = (theme.backgroundMode == ui::themes::TerminalTheme::Image
                       && theme.screenBackgroundOpacity < 1.0);
    session->container->setAttribute(Qt::WA_TranslucentBackground, hasBgImage);
    session->container->setAutoFillBackground(false);

    if (session->terminalView) {
        session->terminalView->applyTerminalTheme(theme);
    }
    QString fgHex = theme.colorGreen.name(QColor::HexRgb);
    QColor bgWithAlpha = theme.backgroundColor;
    if (hasBgImage) {
        bgWithAlpha.setAlphaF(theme.screenBackgroundOpacity);
    }
    QString bgRgba = hasBgImage
        ? QString("rgba(%1,%2,%3,%4)")
              .arg(bgWithAlpha.red()).arg(bgWithAlpha.green())
              .arg(bgWithAlpha.blue()).arg(bgWithAlpha.alphaF(), 0, 'f', 2)
        : theme.backgroundColor.name(QColor::HexRgb);
    if (session->statusBar) {
        session->statusBar->setStyleSheet(
            QString("background-color: %1;").arg(bgRgba));
        session->statusBar->setAttribute(Qt::WA_TranslucentBackground, hasBgImage);
    }
    if (session->coordinatesLabel) {
        session->coordinatesLabel->setStyleSheet(
            QString("color: %1; background-color: %2; padding: 0px;")
                .arg(fgHex, bgRgba));
    }
    if (session->connectionStatus) {
        session->connectionStatus->setTextColor(theme.colorGreen);
    }
    // CRT overlay applies to the entire tab
    if (session->crtOverlay) {
        session->crtOverlay->setScanlineIntensity(theme.crtScanlineIntensity);
        session->crtOverlay->setGlowRadius(theme.crtGlowRadius);
        session->crtOverlay->setGlowColor(theme.colorGreen);
        session->crtOverlay->setCurvature(theme.crtCurvature);
        session->crtOverlay->setPhosphorBloom(theme.crtPhosphorBloom);
        session->crtOverlay->setEnabled(theme.crtEffectEnabled);
        session->crtOverlay->setGeometry(session->container->rect());
        session->crtOverlay->raise();
    }
    session->config.setTerminalThemeId(themeId);

    // Update tab tooltip to show theme name
    int idx = m_sessions.indexOf(session);
    if (idx >= 0) {
        QString host = session->config.hostname();
        m_tabWidget->setTabToolTip(idx,
            QString("%1 — Theme: %2").arg(host.isEmpty() ? "Session" : host,
                                           theme.displayName));
    }
}


void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if (path.endsWith(".5250theme", Qt::CaseInsensitive)
                || path.endsWith(".json", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    int imported = 0;
    for (const QUrl &url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (path.endsWith(".5250theme", Qt::CaseInsensitive)
            || path.endsWith(".json", Qt::CaseInsensitive)) {
            if (mgr.importTheme(path)) {
                ++imported;
            }
        }
    }
    if (imported > 0) {
        ui::widgets::StyledMessageBox::information(this, "Import Theme",
            QString("Imported %1 theme(s) successfully.").arg(imported));
    }
}

/**
 * Intercept context menu events on the tab bar to support per-tab actions.
 *
 * @param obj   The watched object.
 * @param event The incoming event.
 * @return true if the event was handled, false to propagate.
 */
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    // Resize CRT overlay to match tab container; repaint bg image
    if (event->type() == QEvent::Resize) {
        for (auto *s : m_sessions) {
            if (obj == s->container) {
                if (s->crtOverlay) {
                    s->crtOverlay->setGeometry(s->container->rect());
                    s->crtOverlay->raise();
                }
                break;
            }
        }
    }
    // Paint background image behind children in the tab container
    if (event->type() == QEvent::Paint) {
        for (auto *s : m_sessions) {
            if (obj == s->container && !s->bgImage.isNull()) {
                QPainter p(s->container);
                p.setOpacity(s->bgImageOpacity);
                QRect r = s->container->rect();
                switch (s->bgImageLayout) {
                case ui::themes::TerminalTheme::Stretch:
                    // Use cached scaled pixmap; regenerate only when size changes
                    if (s->bgImageScaledSize != r.size()) {
                        s->bgImageScaled = s->bgImage.scaled(r.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                        s->bgImageScaledSize = r.size();
                    }
                    p.drawPixmap(0, 0, s->bgImageScaled);
                    break;
                case ui::themes::TerminalTheme::Tile:
                    for (int y = 0; y < r.height(); y += s->bgImage.height())
                        for (int x = 0; x < r.width(); x += s->bgImage.width())
                            p.drawPixmap(x, y, s->bgImage);
                    break;
                case ui::themes::TerminalTheme::Center:
                    p.drawPixmap((r.width() - s->bgImage.width()) / 2,
                                 (r.height() - s->bgImage.height()) / 2,
                                 s->bgImage);
                    break;
                case ui::themes::TerminalTheme::Fit:
                    if (s->bgImageScaledSize != r.size()) {
                        s->bgImageScaled = s->bgImage.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        s->bgImageScaledSize = r.size();
                    }
                    p.drawPixmap((r.width() - s->bgImageScaled.width()) / 2,
                                 (r.height() - s->bgImageScaled.height()) / 2,
                                 s->bgImageScaled);
                    break;
                }
                break;
            }
        }
    }
    // Middle-click on a tab to close it
    if (obj == m_tabWidget->tabBar() && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            int tabIndex = m_tabWidget->tabBar()->tabAt(me->pos());
            if (tabIndex >= 0) {
                onCloseTabRequested(tabIndex);
                return true;
            }
        }
    }
    if (obj == m_tabWidget->tabBar() && event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *ce = static_cast<QContextMenuEvent *>(event);
        int tabIndex = m_tabWidget->tabBar()->tabAt(ce->pos());
        if (tabIndex >= 0) {
            QMenu menu(this);
            QAction *rename = menu.addAction("Rename");

            // Theme submenu in context menu
            QMenu *themeSubMenu = menu.addMenu("Theme");
            if (tabIndex >= 0 && tabIndex < m_sessions.size()) {
                auto &mgr = ui::themes::TerminalThemeManager::instance();
                QString curId = m_sessions[tabIndex]->config.terminalThemeId();
                for (const QString &id : mgr.availableThemes()) {
                    ui::themes::TerminalTheme t = mgr.theme(id);
                    // Color swatch icon
                    QPixmap swatch(16, 16);
                    swatch.fill(t.backgroundColor);
                    QPainter sp(&swatch);
                    sp.setPen(Qt::NoPen);
                    sp.setBrush(t.colorGreen);
                    sp.drawRect(4, 4, 8, 8);
                    sp.end();

                    QString label = t.displayName;
                    QAction *a = themeSubMenu->addAction(QIcon(swatch), label);
                    a->setData(id);
                    a->setCheckable(true);
                    a->setChecked(id == curId);
                }
            }

            menu.addSeparator();
            QAction *close = menu.addAction("Close");
            QAction *chosen = menu.exec(ce->globalPos());
            if (chosen == rename) {
                onRenameTabRequested(tabIndex);
            } else if (chosen == close) {
                onCloseTabRequested(tabIndex);
            } else if (chosen && chosen->parent() == themeSubMenu) {
                // Theme was chosen from context menu
                QString themeId = chosen->data().toString();
                if (!themeId.isEmpty() && tabIndex >= 0 && tabIndex < m_sessions.size()) {
                    applyThemeToSession(m_sessions[tabIndex], themeId);
                    if (tabIndex == m_activeIndex) {
                        m_currentSession.setTerminalThemeId(themeId);
                    }
                }
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// onClearScreenRequested, onKeyboardUnlockRequested, onControlCharactersReceived,
// processDeferredCC2, onSohReceived, onRollRequested, onWriteErrorCode
// are now in ui::rendering::TN5250CommandHandler

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

