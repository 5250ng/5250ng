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

#include "network/tn5250_qt/devices/devices.h"
#include "session/config.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHash>
#include <QVector>

namespace ui::themes { class TerminalThemeManager; }

class ConnectDialog : public ui::widgets::BaseFramelessDialog {
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
    void onAttachScriptClicked();
    void onDetachScriptClicked();

  private:
    void setupUI();
    void updateUI();
    void rebuildScriptVariableFields();

    QLineEdit *m_hostnameEdit;
    QSpinBox *m_portSpin;
    QCheckBox *m_tlsCheck;
    QCheckBox *m_allowInvalidCertsCheck;
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
    QPushButton *m_loadSessionButton;
    QPushButton *m_deleteSessionButton;
    QGroupBox *m_displayGroup;
    QComboBox *m_codePageCombo;
    QComboBox *m_themeCombo;
    QLabel *m_scriptNameLabel;
    QPushButton *m_attachScriptButton;
    QPushButton *m_detachScriptButton;
    QGroupBox *m_scriptVarsGroup;
    QFormLayout *m_scriptVarsLayout;
    QHash<QString, QLineEdit*> m_scriptVarEdits;

    session::SessionConfig m_currentConfig;
    QVector<tn5250::devices::Device> m_supported;
    int m_customDeviceIndex = -1;
};
