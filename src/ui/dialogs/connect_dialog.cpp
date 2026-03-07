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

#include "connect_dialog.h"
#include "core/codepage.h"
#include "session/manager.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QGroupBox>
#include <QInputDialog>
#include "ui/widgets/Frameless/StyledMessageBox.h"

ConnectDialog::ConnectDialog(QWidget *parent) : ui::widgets::BaseFramelessDialog(parent) {
    setupUI();
    updateUI();
}

ConnectDialog::~ConnectDialog() {}

void ConnectDialog::setupUI() {
    setWindowTitle("Connect to Server");
    setModal(true);
    resize(400, 300);

    QVBoxLayout *mainLayout = contentLayout();
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Session management group
    QGroupBox *sessionGroup = new QGroupBox("Saved Sessions", this);
    QVBoxLayout *sessionLayout = new QVBoxLayout(sessionGroup);

    m_sessionCombo = new QComboBox(this);
    m_sessionCombo->setEditable(false);
    m_sessionCombo->addItem("(New Session)");

    session::SessionManager *sessionManager = new session::SessionManager(this);
    QStringList sessions = sessionManager->listSessions();
    m_sessionCombo->addItems(sessions);

    sessionLayout->addWidget(m_sessionCombo);

    QHBoxLayout *sessionButtonsLayout = new QHBoxLayout();
    m_loadSessionButton = new QPushButton("Load Session", this);
    m_saveSessionButton = new QPushButton("Save Session", this);
    m_deleteSessionButton = new QPushButton("Delete Session", this);
    sessionButtonsLayout->addWidget(m_loadSessionButton);
    sessionButtonsLayout->addWidget(m_saveSessionButton);
    sessionButtonsLayout->addWidget(m_deleteSessionButton);
    sessionLayout->addLayout(sessionButtonsLayout);

    sessionGroup->setLayout(sessionLayout);
    mainLayout->addWidget(sessionGroup);

    // Connection settings group
    QGroupBox *connectionGroup = new QGroupBox("Connection Settings", this);
    QFormLayout *formLayout = new QFormLayout(connectionGroup);

    m_hostnameEdit = new QLineEdit(this);
    m_hostnameEdit->setPlaceholderText("hostname or IP address");
    formLayout->addRow("Hostname:", m_hostnameEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(23);
    formLayout->addRow("Port:", m_portSpin);

    m_tlsCheck = new QCheckBox("Use TLS/SSL", this);
    formLayout->addRow("", m_tlsCheck);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText("(optional - for encrypted sign-on)");
    formLayout->addRow("Username:", m_usernameEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText("(optional - for encrypted sign-on)");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow("Password:", m_passwordEdit);

    // Emulated device dropdown
    m_deviceCombo = new QComboBox(this);
    // Populate supported devices
    for (const auto &dev : tn5250::devices::supportedDevices()) {
        m_supported.append(dev);
        m_deviceCombo->addItem(dev.model);
    }
    m_customDeviceIndex = m_deviceCombo->count();
    m_deviceCombo->addItem("Custom device");
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectDialog::onDeviceComboChanged);
    formLayout->addRow("Emulated Device:", m_deviceCombo);
    // Device name field (always present; enabled only for Custom)
    m_customDeviceEdit = new QLineEdit(this);
    m_customDeviceEdit->setPlaceholderText(
        "Custom device name (e.g., IBM-3179-2)"
    );
    m_customDeviceEdit->setEnabled(false);
    formLayout->addRow("Device name:", m_customDeviceEdit);

    connectionGroup->setLayout(formLayout);
    mainLayout->addWidget(connectionGroup);

    // Display settings group
    m_displayGroup = new QGroupBox("Display Settings", this);
    QFormLayout *displayLayout = new QFormLayout(m_displayGroup);

    // Rows/Columns in a single horizontal line
    QWidget *dimsRow = new QWidget(this);
    QHBoxLayout *dimsLayout = new QHBoxLayout(dimsRow);
    dimsLayout->setContentsMargins(0, 0, 0, 0);
    dimsLayout->setSpacing(8);
    QLabel *rowsLabel = new QLabel("Rows:", this);
    m_rowsSpin = new QSpinBox(this);
    m_rowsSpin->setRange(1, 132);
    m_rowsSpin->setValue(24);
    QLabel *colsLabel = new QLabel("Columns:", this);
    m_colsSpin = new QSpinBox(this);
    m_colsSpin->setRange(1, 200);
    m_colsSpin->setValue(80);
    // Make four equal parts within the hbox
    rowsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_rowsSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    colsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_colsSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    dimsLayout->addWidget(rowsLabel, /*stretch*/ 1);
    dimsLayout->addWidget(m_rowsSpin, /*stretch*/ 1);
    dimsLayout->addWidget(colsLabel, /*stretch*/ 1);
    dimsLayout->addWidget(m_colsSpin, /*stretch*/ 1);
    displayLayout->addRow(dimsRow);

    m_displayGroup->setLayout(displayLayout);
    mainLayout->addWidget(m_displayGroup);

    // Code page group
    QGroupBox *codePageGroup = new QGroupBox("Code Page", this);
    QFormLayout *codePageLayout = new QFormLayout(codePageGroup);
    m_codePageCombo = new QComboBox(this);
    for (auto cpId : core::CodePage::supportedCodePages()) {
        m_codePageCombo->addItem(core::CodePage::codepageName(cpId),
                                 static_cast<int>(cpId));
    }
    codePageLayout->addRow("EBCDIC Code Page:", m_codePageCombo);
    codePageGroup->setLayout(codePageLayout);
    mainLayout->addWidget(codePageGroup);

    // Terminal theme group
    QGroupBox *themeGroup = new QGroupBox("Terminal Theme", this);
    QFormLayout *themeLayout = new QFormLayout(themeGroup);
    m_themeCombo = new QComboBox(this);
    auto &themeMgr = ui::themes::TerminalThemeManager::instance();
    for (const QString &id : themeMgr.availableThemes()) {
        ui::themes::TerminalTheme t = themeMgr.theme(id);
        QString label = t.displayName;
        if (t.builtin)
            label += " (built-in)";
        m_themeCombo->addItem(label, id);
    }
    themeLayout->addRow("Theme:", m_themeCombo);
    themeGroup->setLayout(themeLayout);
    mainLayout->addWidget(themeGroup);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton("Connect", this);
    m_connectButton->setDefault(true);
    m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_connectButton, &QPushButton::clicked, this, &ConnectDialog::onConnectClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ConnectDialog::onCancelClicked);
    connect(m_saveSessionButton, &QPushButton::clicked, this, &ConnectDialog::onSaveSessionClicked);
    connect(m_loadSessionButton, &QPushButton::clicked, this, &ConnectDialog::onLoadSessionClicked);
    connect(m_deleteSessionButton, &QPushButton::clicked, this, &ConnectDialog::onDeleteSessionClicked);
    connect(m_sessionCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged), this, &ConnectDialog::onSessionComboChanged);
    // Initial state: disable delete for "(New Session)"
    m_deleteSessionButton->setEnabled(false);
}

void ConnectDialog::updateUI() {
    m_hostnameEdit->setText(m_currentConfig.hostname());
    m_portSpin->setValue(m_currentConfig.port());
    m_tlsCheck->setChecked(m_currentConfig.useTLS());
    m_usernameEdit->setText(m_currentConfig.username());
    m_passwordEdit->setText(m_currentConfig.password());
    // Select device in combo if supported
    int idx = m_deviceCombo->findText(m_currentConfig.deviceName());
    if (idx >= 0) {
        m_deviceCombo->setCurrentIndex(idx);
        // Ensure display reflects the supported device
        const auto *dev =
            tn5250::devices::findSupportedDevice(m_currentConfig.deviceName());
        if (dev) {
            m_rowsSpin->setValue(dev->lines);
            m_colsSpin->setValue(dev->columns);
        }
        m_customDeviceEdit->setText(m_currentConfig.deviceName());
        m_customDeviceEdit->setEnabled(false);
        m_displayGroup->setEnabled(false);
    } else {
        // Custom device
        m_deviceCombo->setCurrentIndex(m_customDeviceIndex);
        m_customDeviceEdit->setText(m_currentConfig.deviceName());
        m_customDeviceEdit->setEnabled(true);
        m_rowsSpin->setValue(m_currentConfig.screenRows());
        m_colsSpin->setValue(m_currentConfig.screenCols());
        m_displayGroup->setEnabled(true);
    }

    // Code page
    int cpIdx = m_codePageCombo->findData(static_cast<int>(m_currentConfig.codePage()));
    if (cpIdx >= 0) {
        m_codePageCombo->setCurrentIndex(cpIdx);
    }

    // Terminal theme
    int themeIdx = m_themeCombo->findData(m_currentConfig.terminalThemeId());
    if (themeIdx >= 0) {
        m_themeCombo->setCurrentIndex(themeIdx);
    }
}

session::SessionConfig ConnectDialog::getSessionConfig() const {
    session::SessionConfig config;
    config.setName("Current Session");
    config.setHostname(m_hostnameEdit->text());
    config.setPort(static_cast<quint16>(m_portSpin->value()));
    config.setUseTLS(m_tlsCheck->isChecked());
    if (m_deviceCombo->currentIndex() == m_customDeviceIndex) {
        config.setDeviceName(m_customDeviceEdit->text());
    } else {
        config.setDeviceName(m_deviceCombo->currentText());
    }
    config.setScreenRows(m_rowsSpin->value());
    config.setScreenCols(m_colsSpin->value());
    config.setUsername(m_usernameEdit->text());
    config.setPassword(m_passwordEdit->text());
    config.setCodePage(static_cast<core::CodePage::ID>(
        m_codePageCombo->currentData().toInt()));
    config.setTerminalThemeId(m_themeCombo->currentData().toString());
    return config;
}

void ConnectDialog::setSessionConfig(const session::SessionConfig &config) {
    m_currentConfig = config;
    updateUI();
}

void ConnectDialog::loadSession(const QString &sessionName) {
    if (sessionName.isEmpty() || sessionName == "(New Session)") {
        return;
    }

    session::SessionManager sessionManager(this);
    session::SessionConfig config;
    if (sessionManager.loadSession(sessionName, config)) {
        setSessionConfig(config);
        emit sessionSelected(sessionName);
    } else {
        ui::widgets::StyledMessageBox::warning(
            this, "Load Session",
            QString("Failed to load session: %1").arg(sessionName)
        );
    }
}

void ConnectDialog::saveCurrentAsSession() {
    bool ok;
    QString sessionName = QInputDialog::getText(
        this, "Save Session", "Session name:", QLineEdit::Normal,
        m_currentConfig.name(), &ok
    );
    if (!ok || sessionName.isEmpty()) {
        return;
    }

    session::SessionConfig config = getSessionConfig();
    config.setName(sessionName);

    if (!config.isValid()) {
        ui::widgets::StyledMessageBox::warning(this, "Save Session", "Invalid session configuration.");
        return;
    }

    session::SessionManager sessionManager(this);
    if (sessionManager.saveSession(config)) {
        // Update combo box
        if (m_sessionCombo->findText(sessionName) == -1) {
            m_sessionCombo->addItem(sessionName);
        }
        m_sessionCombo->setCurrentText(sessionName);
        ui::widgets::StyledMessageBox::information(
            this, "Save Session",
            QString("Session '%1' saved successfully.").arg(sessionName)
        );
    } else {
        ui::widgets::StyledMessageBox::warning(this, "Save Session", "Failed to save session.");
    }
}

QStringList ConnectDialog::availableSessions() const {
    session::SessionManager sessionManager;
    return sessionManager.listSessions();
}

void ConnectDialog::onConnectClicked() {
    session::SessionConfig config = getSessionConfig();

    if (!config.isValid()) {
        ui::widgets::StyledMessageBox::warning(this, "Connect", "Please fill in all required fields.\n"
                                              "Hostname is required.");
        return;
    }

    emit connectRequested(config);
    accept();
}

void ConnectDialog::onCancelClicked() { reject(); }

void ConnectDialog::onSaveSessionClicked() { saveCurrentAsSession(); }

void ConnectDialog::onLoadSessionClicked() {
    QString sessionName = m_sessionCombo->currentText();
    loadSession(sessionName);
}

void ConnectDialog::onSessionComboChanged(const QString &sessionName) {
    if (sessionName != "(New Session)") {
        loadSession(sessionName);
        m_deleteSessionButton->setEnabled(true);
    } else {
        m_deleteSessionButton->setEnabled(false);
    }
}

void ConnectDialog::onDeviceComboChanged(int index) {
    if (index == m_customDeviceIndex) {
        // Enable custom entry and display settings
        m_customDeviceEdit->setEnabled(true);
        m_displayGroup->setEnabled(true);
        // Keep current rows/cols
        m_customDeviceEdit->clear();
    } else if (index >= 0 && index < m_customDeviceIndex) {
        // Predefined device selected
        m_customDeviceEdit->setEnabled(false);
        m_displayGroup->setEnabled(false);
        const auto &dev = m_supported[index];
        // Update display settings to match selected device
        m_rowsSpin->setValue(dev.lines);
        m_colsSpin->setValue(dev.columns);
        // Reflect device name in the field
        m_customDeviceEdit->setText(dev.model);
    }
}

void ConnectDialog::onDeleteSessionClicked() {
    QString sessionName = m_sessionCombo->currentText();
    if (sessionName.isEmpty() || sessionName == "(New Session)") {
        return;
    }
    auto reply = ui::widgets::StyledMessageBox::question(
        this, "Delete Session", QString("Delete session '%1'?").arg(sessionName));
    if (reply != ui::widgets::StyledMessageBox::Yes) {
        return;
    }
    session::SessionManager mgr(this);
    if (mgr.deleteSession(sessionName)) {
        int idx = m_sessionCombo->findText(sessionName);
        if (idx >= 0) {
            m_sessionCombo->removeItem(idx);
        }
        m_sessionCombo->setCurrentText("(New Session)");
        m_deleteSessionButton->setEnabled(false);
        ui::widgets::StyledMessageBox::information(this, "Delete Session", QString("Session '%1' deleted.").arg(sessionName));
    } else {
        ui::widgets::StyledMessageBox::warning(
            this, "Delete Session",
            QString("Failed to delete session '%1'.").arg(sessionName)
        );
    }
}
