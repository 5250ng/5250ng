#include "main_window.h"
#include <QApplication>
#include <QMessageBox>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_displayWidget(nullptr), m_client(nullptr),
      m_parser(nullptr), m_connected(false) {
  setWindowTitle("5250ng");
  resize(1000, 600);

  setupUI();
  setupMenuBar();
  setupStatusBar();

  // Initialize logger
  core::Logger::instance()->info("5250ng started");
  connect(core::Logger::instance(), &core::Logger::logMessage, this,
          &MainWindow::onLogMessage);
}

MainWindow::~MainWindow() {
  if (m_client) {
    m_client->disconnectFromHost();
  }
}

void MainWindow::setupUI() {
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *layout = new QVBoxLayout(centralWidget);
  layout->setContentsMargins(0, 0, 0, 0);

  m_displayWidget = new display::TN5250Widget(this);
  layout->addWidget(m_displayWidget);

  // Connect display widget input to transport
  connect(m_displayWidget, &display::TN5250Widget::inputReady, this,
          &MainWindow::onInputReady);

  centralWidget->setLayout(layout);
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
}

void MainWindow::setupStatusBar() {
  m_statusLabel = new QLabel("Not connected", this);
  statusBar()->addWidget(m_statusLabel);
  statusBar()->showMessage("Ready");
}

void MainWindow::onConnect() {
  ConnectDialog dialog(this);

  if (dialog.exec() == QDialog::Accepted) {
    core::SessionConfig config = dialog.getSessionConfig();
    connectToServer(config);
  }
}

void MainWindow::onDisconnect() {
  if (m_client) {
    m_client->disconnectFromHost();
  }
}

void MainWindow::onConnectRequested(const core::SessionConfig &config) {
  connectToServer(config);
}

void MainWindow::connectToServer(const core::SessionConfig &config) {
  m_currentSession = config;

  // Create client if needed
  if (!m_client) {
    m_client = new transport::TN5250Client(this);
    m_parser = new transport::ProtocolParser(this);

    connect(m_client, &transport::TN5250Client::connected, this,
            &MainWindow::onConnected);
    connect(m_client, &transport::TN5250Client::disconnected, this,
            &MainWindow::onDisconnected);
    connect(m_client, &transport::TN5250Client::errorOccurred, this,
            &MainWindow::onErrorOccurred);
    connect(m_client, &transport::TN5250Client::dataReceived, this,
            &MainWindow::onDataReceived);

    connect(m_parser, &transport::ProtocolParser::commandReceived, this,
            [this](transport::TN5250Command cmd, const QByteArray &data) {
              // Handle protocol commands - will be implemented in display layer
              // integration
              core::Logger::instance()->debug(
                  QString("Command received: %1").arg(static_cast<int>(cmd)));
            });
  }

  // Configure client
  m_client->setDeviceName(config.deviceName());

  // Configure display
  m_displayWidget->setScreenSize(config.screenRows(), config.screenCols());

  // Connect
  core::Logger::instance()->info(
      QString("Connecting to %1:%2").arg(config.hostname()).arg(config.port()));
  m_client->connectToHost(config.hostname(), config.port(), config.useTLS());

  updateConnectionStatus(false);
}

void MainWindow::onConnected() {
  core::Logger::instance()->info("Connected to TN5250 server");
  updateConnectionStatus(true);
  statusBar()->showMessage("Connected", 3000);
}

void MainWindow::onDisconnected() {
  core::Logger::instance()->info("Disconnected from TN5250 server");
  updateConnectionStatus(false);
  statusBar()->showMessage("Disconnected", 3000);
}

void MainWindow::onErrorOccurred(const QString &error) {
  core::Logger::instance()->error(QString("Connection error: %1").arg(error));
  QMessageBox::warning(this, "Connection Error", error);
  updateConnectionStatus(false);
}

void MainWindow::onDataReceived(const QByteArray &data) {
  // Parse incoming data
  if (m_parser) {
    m_parser->parseData(data);
  }

  // For now, just log - full display integration will be in next phase
  core::Logger::instance()->debug(
      QString("Data received: %1 bytes").arg(data.size()));
}

void MainWindow::onInputReady(const QByteArray &data) {
  if (m_client && m_client->isConnected()) {
    m_client->sendData(data);
    core::Logger::instance()->debug(
        QString("Data sent: %1 bytes").arg(data.size()));
  }
}

void MainWindow::onLogMessage(core::LogLevel level, const QString &message) {
  // Update status bar for important messages
  if (level == core::LogLevel::Error) {
    statusBar()->showMessage(message, 5000);
  }
}

void MainWindow::updateConnectionStatus(bool connected) {
  m_connected = connected;
  m_connectAction->setEnabled(!connected);
  m_disconnectAction->setEnabled(connected);

  if (connected) {
    m_statusLabel->setText(QString("Connected to %1:%2")
                               .arg(m_currentSession.hostname())
                               .arg(m_currentSession.port()));
  } else {
    m_statusLabel->setText("Not connected");
  }
}

void MainWindow::autoConnect(const core::SessionConfig &config) {
  // Connect after a short delay to ensure UI is ready
  QTimer::singleShot(100, this, [this, config]() { connectToServer(config); });
}