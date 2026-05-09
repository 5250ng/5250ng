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
#include "5250script/script_parser.h"
#include "5250script/script_utils.h"
#include "core/codepage.h"
#include "session/manager.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QStandardPaths>
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

    m_allowInvalidCertsCheck = new QCheckBox("Allow invalid TLS certificates (insecure)", this);
    m_allowInvalidCertsCheck->setToolTip(
        "When enabled, self-signed, expired, or hostname-mismatched TLS "
        "certificates are accepted. Leave off unless you trust the host.");
    formLayout->addRow("", m_allowInvalidCertsCheck);

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
    // Custom device type field (enabled only when "Custom device" is selected)
    m_customDeviceEdit = new QLineEdit(this);
    m_customDeviceEdit->setPlaceholderText("e.g., IBM-3179-2");
    m_customDeviceEdit->setEnabled(false);
    m_customDeviceLabel = new QLabel("Device Type:");
    formLayout->addRow(m_customDeviceLabel, m_customDeviceEdit);
    m_customDeviceLabel->setVisible(false);
    m_customDeviceEdit->setVisible(false);

    // Device name (workstation identifier sent as DEVNAME; empty = auto-assigned)
    m_deviceNameEdit = new QLineEdit(this);
    m_deviceNameEdit->setPlaceholderText("(auto-assigned by server)");
    m_deviceNameEdit->setMaxLength(10); // IBM i device names max 10 chars
    formLayout->addRow("Device Name:", m_deviceNameEdit);

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

    // PC Command Execution (STRPCCMD) — three-way policy. Off by default
    // because this is the mechanism behind CVE-2005-0868: the host can ask
    // the client to run any command its user can run.
    QGroupBox *pcCmdGroup = new QGroupBox("PC Command Execution (STRPCCMD)", this);
    QVBoxLayout *pcCmdLayout = new QVBoxLayout(pcCmdGroup);
    QLabel *pcCmdWarning = new QLabel(
        "Controls whether this host can ask 5250ng to run a command on your PC. "
        "STRPCCMD is the mechanism behind CVE-2005-0868 — leave on \"Deny\" "
        "unless you trust this host.",
        this);
    pcCmdWarning->setWordWrap(true);
    pcCmdLayout->addWidget(pcCmdWarning);

    m_pcCmdDenyRadio = new QRadioButton(
        "Deny silently: refuse all PC commands (recommended)", this);
    m_pcCmdDenyAlertRadio = new QRadioButton(
        "Deny and alert: refuse, but show a notification when the host attempts a command", this);
    m_pcCmdPromptRadio = new QRadioButton(
        "Allow with confirmation: prompt before running each command", this);
    m_pcCmdAllowAlwaysRadio = new QRadioButton(
        "Allow without prompting: run every command silently (insecure)", this);
    m_pcCmdDenyRadio->setToolTip(
        "Default. The host's STRPCCMD requests are silently refused; the host "
        "still receives an ENTER reply so its CL program continues.");
    m_pcCmdDenyAlertRadio->setToolTip(
        "The host's STRPCCMD requests are refused, but a notification dialog "
        "is shown each time so you know the host attempted to run something. "
        "Useful when you want to monitor a host without trusting it.");
    m_pcCmdPromptRadio->setToolTip(
        "Each STRPCCMD shows a modal with the command string and Allow/Deny "
        "before anything runs.");
    m_pcCmdAllowAlwaysRadio->setToolTip(
        "Every STRPCCMD runs immediately with no user confirmation. Anything "
        "the host sends will execute as you. Only use on hosts you fully "
        "control.");
    pcCmdLayout->addWidget(m_pcCmdDenyRadio);
    pcCmdLayout->addWidget(m_pcCmdDenyAlertRadio);
    pcCmdLayout->addWidget(m_pcCmdPromptRadio);
    pcCmdLayout->addWidget(m_pcCmdAllowAlwaysRadio);
    // Default selection mirrors the SessionConfig default (DenyAndAlert).
    m_pcCmdDenyAlertRadio->setChecked(true);
    pcCmdGroup->setLayout(pcCmdLayout);
    mainLayout->addWidget(pcCmdGroup);

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

    // Startup script group
    QGroupBox *scriptGroup = new QGroupBox("Startup Script", this);
    QFormLayout *scriptLayout = new QFormLayout(scriptGroup);
    m_scriptNameLabel = new QLabel("(None)", this);
    scriptLayout->addRow("Script:", m_scriptNameLabel);
    QHBoxLayout *scriptButtonsLayout = new QHBoxLayout();
    m_attachScriptButton = new QPushButton("Attach Script...", this);
    m_detachScriptButton = new QPushButton("Detach", this);
    m_detachScriptButton->setEnabled(false);
    scriptButtonsLayout->addWidget(m_attachScriptButton);
    scriptButtonsLayout->addWidget(m_detachScriptButton);
    scriptLayout->addRow(scriptButtonsLayout);
    scriptGroup->setLayout(scriptLayout);
    mainLayout->addWidget(scriptGroup);

    // Script variables group (dynamic, initially hidden)
    m_scriptVarsGroup = new QGroupBox("Script Variables", this);
    m_scriptVarsLayout = new QFormLayout(m_scriptVarsGroup);
    m_scriptVarsGroup->setLayout(m_scriptVarsLayout);
    m_scriptVarsGroup->setVisible(false);
    mainLayout->addWidget(m_scriptVarsGroup);

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
    connect(m_attachScriptButton, &QPushButton::clicked, this, &ConnectDialog::onAttachScriptClicked);
    connect(m_detachScriptButton, &QPushButton::clicked, this, &ConnectDialog::onDetachScriptClicked);
    connect(m_sessionCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged), this, &ConnectDialog::onSessionComboChanged);
    // Initial state: disable delete for "(New Session)"
    m_deleteSessionButton->setEnabled(false);
}

void ConnectDialog::updateUI() {
    m_hostnameEdit->setText(m_currentConfig.hostname());
    m_portSpin->setValue(m_currentConfig.port());
    m_tlsCheck->setChecked(m_currentConfig.useTLS());
    m_allowInvalidCertsCheck->setChecked(m_currentConfig.allowInvalidCertificates());
    switch (m_currentConfig.pcCommandPolicy()) {
    case session::PcCommandPolicy::Deny:
        m_pcCmdDenyRadio->setChecked(true);
        break;
    case session::PcCommandPolicy::DenyAndAlert:
        m_pcCmdDenyAlertRadio->setChecked(true);
        break;
    case session::PcCommandPolicy::AllowWithPrompt:
        m_pcCmdPromptRadio->setChecked(true);
        break;
    case session::PcCommandPolicy::AllowAlways:
        m_pcCmdAllowAlwaysRadio->setChecked(true);
        break;
    }
    m_usernameEdit->setText(m_currentConfig.username());
    m_passwordEdit->setText(m_currentConfig.password());
    // Select device type in combo if supported
    int idx = m_deviceCombo->findText(m_currentConfig.deviceType());
    if (idx >= 0) {
        m_deviceCombo->setCurrentIndex(idx);
        const auto *dev =
            tn5250::devices::findSupportedDevice(m_currentConfig.deviceType());
        if (dev) {
            m_rowsSpin->setValue(dev->lines);
            m_colsSpin->setValue(dev->columns);
        }
        m_customDeviceEdit->setText(m_currentConfig.deviceType());
        m_customDeviceEdit->setEnabled(false);
        m_customDeviceLabel->setVisible(false);
        m_customDeviceEdit->setVisible(false);
        m_displayGroup->setEnabled(false);
    } else {
        // Custom device type
        m_deviceCombo->setCurrentIndex(m_customDeviceIndex);
        m_customDeviceEdit->setText(m_currentConfig.deviceType());
        m_customDeviceEdit->setEnabled(true);
        m_customDeviceLabel->setVisible(true);
        m_customDeviceEdit->setVisible(true);
        m_rowsSpin->setValue(m_currentConfig.screenRows());
        m_colsSpin->setValue(m_currentConfig.screenCols());
        m_displayGroup->setEnabled(true);
    }

    // Device name (workstation identifier)
    m_deviceNameEdit->setText(m_currentConfig.deviceName());

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

    // Startup script
    if (!m_currentConfig.startupScriptSource().isEmpty()) {
        m_scriptNameLabel->setText(m_currentConfig.startupScriptName());
        m_detachScriptButton->setEnabled(true);
    } else {
        m_scriptNameLabel->setText("(None)");
        m_detachScriptButton->setEnabled(false);
    }
    rebuildScriptVariableFields();
}

session::SessionConfig ConnectDialog::getSessionConfig() const {
    session::SessionConfig config;
    config.setName("Current Session");
    config.setHostname(m_hostnameEdit->text());
    config.setPort(static_cast<quint16>(m_portSpin->value()));
    config.setUseTLS(m_tlsCheck->isChecked());
    config.setAllowInvalidCertificates(m_allowInvalidCertsCheck->isChecked());
    if (m_pcCmdAllowAlwaysRadio->isChecked()) {
        config.setPcCommandPolicy(session::PcCommandPolicy::AllowAlways);
    } else if (m_pcCmdPromptRadio->isChecked()) {
        config.setPcCommandPolicy(session::PcCommandPolicy::AllowWithPrompt);
    } else if (m_pcCmdDenyAlertRadio->isChecked()) {
        config.setPcCommandPolicy(session::PcCommandPolicy::DenyAndAlert);
    } else {
        config.setPcCommandPolicy(session::PcCommandPolicy::Deny);
    }
    if (m_deviceCombo->currentIndex() == m_customDeviceIndex) {
        config.setDeviceType(m_customDeviceEdit->text());
    } else {
        config.setDeviceType(m_deviceCombo->currentText());
    }
    config.setDeviceName(m_deviceNameEdit->text().trimmed());
    config.setScreenRows(m_rowsSpin->value());
    config.setScreenCols(m_colsSpin->value());
    config.setUsername(m_usernameEdit->text());
    config.setPassword(m_passwordEdit->text());
    config.setCodePage(static_cast<core::CodePage::ID>(
        m_codePageCombo->currentData().toInt()));
    config.setTerminalThemeId(m_themeCombo->currentData().toString());
    config.setStartupScriptSource(m_currentConfig.startupScriptSource());
    config.setStartupScriptName(m_currentConfig.startupScriptName());
    QHash<QString, QString> vars;
    for (auto it = m_scriptVarEdits.constBegin(); it != m_scriptVarEdits.constEnd(); ++it)
        vars[it.key()] = it.value()->text();
    config.setSessionVariables(vars);
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
        // Show and enable custom device type field
        m_customDeviceLabel->setVisible(true);
        m_customDeviceEdit->setVisible(true);
        m_customDeviceEdit->setEnabled(true);
        m_displayGroup->setEnabled(true);
        m_customDeviceEdit->clear();
    } else if (index >= 0 && index < m_customDeviceIndex) {
        // Hide custom device type field for predefined devices
        m_customDeviceLabel->setVisible(false);
        m_customDeviceEdit->setVisible(false);
        m_customDeviceEdit->setEnabled(false);
        m_displayGroup->setEnabled(false);
        const auto &dev = m_supported[index];
        m_rowsSpin->setValue(dev.lines);
        m_colsSpin->setValue(dev.columns);
        m_customDeviceEdit->setText(dev.model);
    }
}

void ConnectDialog::onAttachScriptClicked() {
    QString startDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       + "/scripts";
    if (!QDir(startDir).exists())
        startDir = QDir::homePath();

    QString filePath = QFileDialog::getOpenFileName(
        this, "Attach Startup Script", startDir, "5250 Scripts (*.5250script);;All Files (*)");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui::widgets::StyledMessageBox::warning(this, "Attach Script", "Failed to read script file.");
        return;
    }
    QString source = QString::fromUtf8(file.readAll());
    file.close();

    // Parse script and check for errors
    core::scripting::ScriptParser parser;
    auto result = parser.parse(source);
    if (result.hasErrors()) {
        QStringList msgs;
        for (const auto &err : result.errors)
            msgs << QString("Line %1: %2").arg(err.line).arg(err.message);
        ui::widgets::StyledMessageBox::warning(
            this, "Script Error",
            QString("The script contains errors:\n\n%1").arg(msgs.join("\n")));
        return;
    }

    m_currentConfig.setStartupScriptSource(source);
    m_currentConfig.setStartupScriptName(QFileInfo(filePath).fileName());
    m_scriptNameLabel->setText(m_currentConfig.startupScriptName());
    m_detachScriptButton->setEnabled(true);
    rebuildScriptVariableFields();
}

void ConnectDialog::onDetachScriptClicked() {
    m_currentConfig.setStartupScriptSource(QString());
    m_currentConfig.setStartupScriptName(QString());
    m_currentConfig.setSessionVariables({});
    m_scriptNameLabel->setText("(None)");
    m_detachScriptButton->setEnabled(false);
    rebuildScriptVariableFields();
}

void ConnectDialog::rebuildScriptVariableFields() {
    // Clear existing fields
    while (m_scriptVarsLayout->count() > 0) {
        QLayoutItem *item = m_scriptVarsLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_scriptVarEdits.clear();

    QStringList varNames = core::scripting::extractSessionVariables(
        m_currentConfig.startupScriptSource());

    if (varNames.isEmpty()) {
        m_scriptVarsGroup->setVisible(false);
        return;
    }

    QHash<QString, QString> savedVars = m_currentConfig.sessionVariables();
    for (const QString &varName : varNames) {
        // Derive label from variable name: $SESSION_USERNAME -> "USERNAME"
        QString label = varName.mid(9); // skip "$SESSION_"

        QLineEdit *edit = new QLineEdit(m_scriptVarsGroup);
        edit->setText(savedVars.value(varName));
        m_scriptVarsLayout->addRow(label, edit);
        m_scriptVarEdits[varName] = edit;
    }
    m_scriptVarsGroup->setVisible(true);
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
