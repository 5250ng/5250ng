// 5250ng - A modern IBM TN5250 terminal emulator                                                                                                                                                            
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)                                                                                                                                                          
//                                                                                                                                                                                                           
// This program is free software: you can redistribute it and/or modify                                                                                                                                      
// it under the terms of the GNU General Public License as published by                                                                                                                                      
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.                                                                                                                                                                       
//                                                                                                                                                                                                           
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "network/tn5250_qt/client/client.h"
#include "network/tn5250_qt/devices/devices.h"
#include "session/config.h"
#include <QtUiStyle/BaseFramelessDialog.h>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHash>
#include <QPointer>
#include <QVector>

namespace ui::themes { class TerminalThemeManager; }

class ConnectDialog : public qt_ui_style::BaseFramelessDialog {
    Q_OBJECT

  public:
    explicit ConnectDialog(QWidget *parent = nullptr);
    ~ConnectDialog();

    // Get/set session configuration
    session::SessionConfig getSessionConfig() const;
    void setSessionConfig(const session::SessionConfig &config);

    // Session management
    void loadSession(const QString &sessionName);
    void saveCurrentSession();
    void saveCurrentAsSession();
    QStringList availableSessions() const;

    // True when the name belongs to a saved session rather than one of the
    // placeholder names used for unsaved / command-line sessions.
    static bool isSavedSessionName(const QString &name);

    // Prompt for a session name and save `config` under it, asking before
    // overwriting a different saved session. On success `config` carries the
    // chosen name and true is returned.
    static bool promptSaveSessionAs(QWidget *parent, session::SessionConfig &config);

  signals:
    void connectRequested(const session::SessionConfig &config);
    void sessionSelected(const QString &sessionName);

  private slots:
    void onConnectClicked();
    void onCancelClicked();
    void onSaveSessionClicked();
    void onSaveSessionAsClicked();
    void onLoadSessionClicked();
    void onSessionComboChanged(const QString &sessionName);
    void onDeviceComboChanged(int index);
    void onTestConnectionClicked();
    void onDeleteSessionClicked();
    void onScriptComboChanged(int index);
    void onBrowseScriptClicked();
    void onNewScriptClicked();
    void onEditScriptClicked();
    void onReloadScriptClicked();

  private:
    void setupUI();
    void updateUI();
    void rebuildScriptVariableFields();
    bool attachScriptFromFile(const QString &filePath, bool quiet);
    void detachScript();
    void populateScriptCombo();
    void updateScriptControls();

    QLineEdit *m_hostnameEdit;
    QSpinBox *m_portSpin;
    QCheckBox *m_tlsCheck;
    QCheckBox *m_allowInvalidCertsCheck;
    QRadioButton *m_pcCmdDenyRadio;
    QRadioButton *m_pcCmdDenyAlertRadio;
    QRadioButton *m_pcCmdPromptRadio;
    QRadioButton *m_pcCmdAllowAlwaysRadio;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QComboBox *m_deviceCombo;
    QLineEdit *m_customDeviceEdit;   // Device type (only for "Custom device")
    QLabel *m_customDeviceLabel;     // Label for custom device type row
    QLineEdit *m_deviceNameEdit;     // Workstation name (DEVNAME)
    QSpinBox *m_rowsSpin;
    QSpinBox *m_colsSpin;
    QComboBox *m_sessionCombo;
    QPushButton *m_connectButton;
    QPushButton *m_cancelButton;
    QPushButton *m_saveSessionButton;
    QPushButton *m_saveSessionAsButton;
    QPushButton *m_loadSessionButton;
    QPushButton *m_deleteSessionButton;
    QGroupBox *m_displayGroup;
    QComboBox *m_codePageCombo;
    QComboBox *m_themeCombo;
    QComboBox *m_scriptCombo;
    QPushButton *m_browseScriptButton;
    QPushButton *m_newScriptButton;
    QPushButton *m_editScriptButton;
    QPushButton *m_reloadScriptButton;
    QLabel *m_scriptSourceLabel;
    QCheckBox *m_scriptLinkedCheck;
    QComboBox *m_scriptTriggerCombo;
    QCheckBox *m_scriptOnReconnectCheck;
    QPlainTextEdit *m_scriptPreview;
    QGroupBox *m_scriptVarsGroup;
    QFormLayout *m_scriptVarsLayout;
    QHash<QString, QLineEdit*> m_scriptVarEdits;
    QPushButton *m_testConnectionButton;
    QLabel *m_testConnectionResult;
    QPointer<tn5250::client::TN5250Client> m_testClient;
    void finishConnectionTest(const QString &message, bool ok);
    QSpinBox *m_connectTimeoutSpin;
    QSpinBox *m_keepaliveSpin;
    QCheckBox *m_autoReconnectCheck;
    QSpinBox *m_reconnectDelaySpin;
    QSpinBox *m_reconnectAttemptsSpin;
    QLineEdit *m_tlsCaFileEdit;
    QLineEdit *m_tlsClientCertEdit;
    QLineEdit *m_tlsClientKeyEdit;
    QTreeWidget *m_categoryTree;
    QStackedWidget *m_pages;

    session::SessionConfig m_currentConfig;
    // Name of the saved session currently loaded in the form; empty for a
    // new, not-yet-saved session.
    QString m_savedSessionName;
    QVector<tn5250::devices::Device> m_supported;
    int m_customDeviceIndex = -1;
};
