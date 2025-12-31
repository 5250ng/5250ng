#pragma once

#include "../core/logger.h"
#include "../core/session_config.h"
#include "../display/tn5250_widget.h"
#include "../transport/protocol_parser.h"
#include "../transport/tn5250_client.h"
#include "connect_dialog.h"
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  // Auto-connect on startup
  void autoConnect(const core::SessionConfig &config);

private slots:
  void onConnect();
  void onDisconnect();
  void onConnectRequested(const core::SessionConfig &config);
  void onConnected();
  void onDisconnected();
  void onErrorOccurred(const QString &error);
  void onDataReceived(const QByteArray &data);
  void onInputReady(const QByteArray &data);
  void onLogMessage(core::LogLevel level, const QString &message);

private:
  void setupUI();
  void setupMenuBar();
  void setupStatusBar();
  void connectToServer(const core::SessionConfig &config);
  void updateConnectionStatus(bool connected);

  display::TN5250Widget *m_displayWidget;
  transport::TN5250Client *m_client;
  transport::ProtocolParser *m_parser;
  core::SessionConfig m_currentSession;

  // UI elements
  QAction *m_connectAction;
  QAction *m_disconnectAction;
  QAction *m_exitAction;
  QLabel *m_statusLabel;

  bool m_connected;
};