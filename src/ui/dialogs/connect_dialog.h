#pragma once

#include "network/tn5250/devices/devices.h"
#include "session/config.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVector>

class ConnectDialog : public QDialog {
    Q_OBJECT

  public:
    explicit ConnectDialog(QWidget *parent = nullptr);
    ~ConnectDialog();

    // Get/set session configuration
    session::SessionConfig getSessionConfig() const;
    void setSessionConfig(const session::SessionConfig &config);

    // Session management
    void loadSession(const QString &sessionName);
    void saveCurrentAsSession();
    QStringList availableSessions() const;

  signals:
    void connectRequested(const session::SessionConfig &config);
    void sessionSelected(const QString &sessionName);

  private slots:
    void onConnectClicked();
    void onCancelClicked();
    void onSaveSessionClicked();
    void onLoadSessionClicked();
    void onSessionComboChanged(const QString &sessionName);
    void onDeviceComboChanged(int index);
    void onDeleteSessionClicked();

  private:
    void setupUI();
    void updateUI();

    QLineEdit *m_hostnameEdit;
    QSpinBox *m_portSpin;
    QCheckBox *m_tlsCheck;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QComboBox *m_deviceCombo;
    QLineEdit *m_customDeviceEdit;
    QSpinBox *m_rowsSpin;
    QSpinBox *m_colsSpin;
    QComboBox *m_sessionCombo;
    QPushButton *m_connectButton;
    QPushButton *m_cancelButton;
    QPushButton *m_saveSessionButton;
    QPushButton *m_loadSessionButton;
    QPushButton *m_deleteSessionButton;
    QGroupBox *m_displayGroup;
    QComboBox *m_codePageCombo;

    session::SessionConfig m_currentConfig;
    QVector<tn5250::devices::Device> m_supported;
    int m_customDeviceIndex = -1;
};
