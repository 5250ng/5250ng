#include "../core/session_manager.h"
#include "dialogs/settings_dialog.h"
#include "main_window.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabBar>
#include <QTimer>
#include <QWidgetAction>
#include <climits>

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setMovable(true);
    m_tabWidget->setTabsClosable(true);
    layout->addWidget(m_tabWidget, 1); // Stretch factor 1 to fill space
    m_activeIndex = -1;
    connect(m_tabWidget, &QTabWidget::currentChanged, this,
            &MainWindow::onCurrentTabChanged);
    connect(m_tabWidget->tabBar(), &QTabBar::tabMoved, this,
            &MainWindow::onTabMoved);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this,
            &MainWindow::onCloseTabRequested);
    m_tabWidget->tabBar()->installEventFilter(this);

    // Active session pointers are null until a tab is created
    m_displayWidget = nullptr;
    m_client = nullptr;
    m_parser = nullptr;

    // Empty placeholder (visible when no tabs)
    m_emptyPlaceholder = new QWidget(this);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyPlaceholder);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->addStretch();
    QLabel *emptyText = new QLabel("No sessions open.\nYou can create a new "
                                   "session by clicking the button below.",
                                   this);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setStyleSheet("color: #cccccc; font-size: 16px;");
    emptyLayout->addWidget(emptyText, 0, Qt::AlignHCenter);
    emptyLayout->addSpacing(8);
    QPushButton *newSessionBtn = new QPushButton("Create New Session", this);
    newSessionBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(newSessionBtn, &QPushButton::clicked, this,
            &MainWindow::onNewSession);
    emptyLayout->addWidget(newSessionBtn, 0, Qt::AlignHCenter);
    // hrule
    emptyLayout->addSpacing(12);
    QFrame *hr = new QFrame(this);
    hr->setFrameShape(QFrame::HLine);
    hr->setFrameShadow(QFrame::Sunken);
    emptyLayout->addWidget(hr);
    emptyLayout->addSpacing(8);
    // Open saved session label and dropdown
    QLabel *openLabel = new QLabel("Open saved session", this);
    openLabel->setAlignment(Qt::AlignCenter);
    openLabel->setStyleSheet("color: #cccccc;");
    emptyLayout->addWidget(openLabel, 0, Qt::AlignHCenter);
    m_emptyOpenCombo = new QComboBox(this);
    m_emptyOpenCombo->setMinimumWidth(320);
    m_emptyOpenCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_emptyOpenCombo->addItem("(Open saved session)");
    connect(m_emptyOpenCombo, &QComboBox::currentTextChanged, this,
            &MainWindow::onQuickOpenChanged);
    emptyLayout->addWidget(m_emptyOpenCombo, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    layout->addWidget(m_emptyPlaceholder, 1);

    // Connect to screen size changes to update font
    connect(m_displayWidget, &display::TN5250Widget::screenSizeChanged, this,
            &MainWindow::updateCursorCoordinatesFont);

    // Initialize coordinates and font
    updateCursorCoordinatesFont();
    updateCursorCoordinates();

    centralWidget->setLayout(layout);
    updateEmptyState();
}

void MainWindow::setupMenuBar() {
    QMenu *fileMenu = menuBar()->addMenu("&File");

    m_connectAction =
        fileMenu->addAction("&Connect...", this, &MainWindow::onConnect);
    m_connectAction->setShortcut(QKeySequence::New);

    m_disconnectAction =
        fileMenu->addAction("&Disconnect", this, &MainWindow::onDisconnect);
    m_disconnectAction->setEnabled(false);

    fileMenu->addSeparator();

    m_exitAction = fileMenu->addAction("E&xit", this, &QWidget::close);
    m_exitAction->setShortcut(QKeySequence::Quit);
    // Settings under File menu
    fileMenu->addSeparator();
    fileMenu->addAction("&Settings...", this, &MainWindow::onOpenSettings);

    // Session menu
    QMenu *sessionMenu = menuBar()->addMenu("&Session");
    QAction *newSessionAction =
        sessionMenu->addAction("&New", this, &MainWindow::onNewSession);
    newSessionAction->setShortcut(QKeySequence::AddTab);
    sessionMenu->addAction("&Settings", this, &MainWindow::onSessionSettings);

    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("&Take a Screenshot", this,
                         &MainWindow::onTakeScreenshot);

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::onAbout);

    // Quick open saved sessions submenu inside Session menu
    m_quickOpenMenu = sessionMenu->addMenu("Open saved session");
    connect(m_quickOpenMenu, &QMenu::aboutToShow, this,
            &MainWindow::rebuildQuickOpenMenu);
    connect(m_quickOpenMenu, &QMenu::triggered, this,
            &MainWindow::onSavedSessionChosen);
}

void MainWindow::setupStatusBar() {
    // Create status indicator (colored round light)
    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setFixedSize(12, 12);
    m_statusIndicator->setObjectName("StatusIndicator");
    m_statusIndicator->setProperty("state", "Disconnected");
    m_statusIndicator->setToolTip("Not connected");

    // Create status text label
    m_statusText = new QLabel("Not connected", this);

    // Add to status bar
    statusBar()->addPermanentWidget(m_statusIndicator);
    statusBar()->addPermanentWidget(m_statusText);

    // Set initial state
    updateStatusIndicator(
        tn5250::client::TN5250Client::ConnectionState::Disconnected);
}