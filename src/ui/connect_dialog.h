#pragma once

#include "../core/session_config.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

class ConnectDialog : public QDialog {
  Q_OBJECT

public:
  explicit ConnectDialog(QWidget *parent = nullptr);
  ~ConnectDialog();

  // Get/set session configuration
  core::SessionConfig getSessionConfig() const;
  void setSessionConfig(const core::SessionConfig &config);

  // Session management
  void loadSession(const QString &sessionName);
  void saveCurrentAsSession();
  QStringList availableSessions() const;

signals:
  void connectRequested(const core::SessionConfig &config);
  void sessionSelected(const QString &sessionName);

private slots:
  void onConnectClicked();
  void onCancelClicked();
  void onSaveSessionClicked();
  void onLoadSessionClicked();
  void onSessionComboChanged(const QString &sessionName);

private:
  void setupUI();
  void updateUI();

  QLineEdit *m_hostnameEdit;
  QSpinBox *m_portSpin;
  QCheckBox *m_tlsCheck;
  QLineEdit *m_deviceNameEdit;
  QSpinBox *m_rowsSpin;
  QSpinBox *m_colsSpin;
  QComboBox *m_sessionCombo;
  QPushButton *m_connectButton;
  QPushButton *m_cancelButton;
  QPushButton *m_saveSessionButton;
  QPushButton *m_loadSessionButton;

  core::SessionConfig m_currentConfig;
};
