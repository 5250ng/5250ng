#include "connect_dialog.h"
#include "../core/session_manager.h"
#include <QMessageBox>
#include <QGroupBox>
#include <QInputDialog>

ConnectDialog::ConnectDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    updateUI();
}

ConnectDialog::~ConnectDialog() {
}

void ConnectDialog::setupUI() {
    setWindowTitle("Connect to Server");
    setModal(true);
    resize(400, 300);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Connection settings group
    QGroupBox* connectionGroup = new QGroupBox("Connection Settings", this);
    QFormLayout* formLayout = new QFormLayout(connectionGroup);
    
    m_hostnameEdit = new QLineEdit(this);
    m_hostnameEdit->setPlaceholderText("hostname or IP address");
    formLayout->addRow("Hostname:", m_hostnameEdit);
    
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(23);
    formLayout->addRow("Port:", m_portSpin);
    
    m_tlsCheck = new QCheckBox("Use TLS/SSL", this);
    formLayout->addRow("", m_tlsCheck);
    
    m_deviceNameEdit = new QLineEdit(this);
    m_deviceNameEdit->setText("QT5250");
    m_deviceNameEdit->setPlaceholderText("Device name");
    formLayout->addRow("Device Name:", m_deviceNameEdit);
    
    connectionGroup->setLayout(formLayout);
    mainLayout->addWidget(connectionGroup);
    
    // Display settings group
    QGroupBox* displayGroup = new QGroupBox("Display Settings", this);
    QFormLayout* displayLayout = new QFormLayout(displayGroup);
    
    m_rowsSpin = new QSpinBox(this);
    m_rowsSpin->setRange(1, 132);
    m_rowsSpin->setValue(24);
    displayLayout->addRow("Rows:", m_rowsSpin);
    
    m_colsSpin = new QSpinBox(this);
    m_colsSpin->setRange(1, 200);
    m_colsSpin->setValue(80);
    displayLayout->addRow("Columns:", m_colsSpin);
    
    displayGroup->setLayout(displayLayout);
    mainLayout->addWidget(displayGroup);
    
    // Session management group
    QGroupBox* sessionGroup = new QGroupBox("Session Management", this);
    QVBoxLayout* sessionLayout = new QVBoxLayout(sessionGroup);
    
    m_sessionCombo = new QComboBox(this);
    m_sessionCombo->setEditable(false);
    m_sessionCombo->addItem("(New Session)");
    
    core::SessionManager* sessionManager = new core::SessionManager(this);
    QStringList sessions = sessionManager->listSessions();
    m_sessionCombo->addItems(sessions);
    
    sessionLayout->addWidget(new QLabel("Saved Sessions:", this));
    sessionLayout->addWidget(m_sessionCombo);
    
    QHBoxLayout* sessionButtonsLayout = new QHBoxLayout();
    m_loadSessionButton = new QPushButton("Load Session", this);
    m_saveSessionButton = new QPushButton("Save Session", this);
    sessionButtonsLayout->addWidget(m_loadSessionButton);
    sessionButtonsLayout->addWidget(m_saveSessionButton);
    sessionLayout->addLayout(sessionButtonsLayout);
    
    sessionGroup->setLayout(sessionLayout);
    mainLayout->addWidget(sessionGroup);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton("Connect", this);
    m_connectButton->setDefault(true);
    m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
    
    // Connect signals
    connect(m_connectButton, &QPushButton::clicked, this, &ConnectDialog::onConnectClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ConnectDialog::onCancelClicked);
    connect(m_saveSessionButton, &QPushButton::clicked, this, &ConnectDialog::onSaveSessionClicked);
    connect(m_loadSessionButton, &QPushButton::clicked, this, &ConnectDialog::onLoadSessionClicked);
    connect(m_sessionCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &ConnectDialog::onSessionComboChanged);
}

void ConnectDialog::updateUI() {
    m_hostnameEdit->setText(m_currentConfig.hostname());
    m_portSpin->setValue(m_currentConfig.port());
    m_tlsCheck->setChecked(m_currentConfig.useTLS());
    m_deviceNameEdit->setText(m_currentConfig.deviceName());
    m_rowsSpin->setValue(m_currentConfig.screenRows());
    m_colsSpin->setValue(m_currentConfig.screenCols());
}

core::SessionConfig ConnectDialog::getSessionConfig() const {
    core::SessionConfig config;
    config.setName("Current Session");
    config.setHostname(m_hostnameEdit->text());
    config.setPort(static_cast<quint16>(m_portSpin->value()));
    config.setUseTLS(m_tlsCheck->isChecked());
    config.setDeviceName(m_deviceNameEdit->text());
    config.setScreenRows(m_rowsSpin->value());
    config.setScreenCols(m_colsSpin->value());
    return config;
}

void ConnectDialog::setSessionConfig(const core::SessionConfig& config) {
    m_currentConfig = config;
    updateUI();
}

void ConnectDialog::loadSession(const QString& sessionName) {
    if (sessionName.isEmpty() || sessionName == "(New Session)") {
        return;
    }
    
    core::SessionManager* sessionManager = new core::SessionManager(this);
    core::SessionConfig config;
    if (sessionManager->loadSession(sessionName, config)) {
        setSessionConfig(config);
        emit sessionSelected(sessionName);
    } else {
        QMessageBox::warning(this, "Load Session", 
                            QString("Failed to load session: %1").arg(sessionName));
    }
}

void ConnectDialog::saveCurrentAsSession() {
    bool ok;
    QString sessionName = QInputDialog::getText(this, "Save Session",
                                                "Session name:",
                                                QLineEdit::Normal,
                                                m_currentConfig.name(),
                                                &ok);
    if (!ok || sessionName.isEmpty()) {
        return;
    }
    
    core::SessionConfig config = getSessionConfig();
    config.setName(sessionName);
    
    if (!config.isValid()) {
        QMessageBox::warning(this, "Save Session", "Invalid session configuration.");
        return;
    }
    
    core::SessionManager* sessionManager = new core::SessionManager(this);
    if (sessionManager->saveSession(config)) {
        // Update combo box
        if (m_sessionCombo->findText(sessionName) == -1) {
            m_sessionCombo->addItem(sessionName);
        }
        m_sessionCombo->setCurrentText(sessionName);
        QMessageBox::information(this, "Save Session", 
                                QString("Session '%1' saved successfully.").arg(sessionName));
    } else {
        QMessageBox::warning(this, "Save Session", "Failed to save session.");
    }
}

QStringList ConnectDialog::availableSessions() const {
    core::SessionManager* sessionManager = new core::SessionManager(const_cast<ConnectDialog*>(this));
    return sessionManager->listSessions();
}

void ConnectDialog::onConnectClicked() {
    core::SessionConfig config = getSessionConfig();
    
    if (!config.isValid()) {
        QMessageBox::warning(this, "Connect", 
                            "Please fill in all required fields.\n"
                            "Hostname is required.");
        return;
    }
    
    emit connectRequested(config);
    accept();
}

void ConnectDialog::onCancelClicked() {
    reject();
}

void ConnectDialog::onSaveSessionClicked() {
    saveCurrentAsSession();
}

void ConnectDialog::onLoadSessionClicked() {
    QString sessionName = m_sessionCombo->currentText();
    loadSession(sessionName);
}

void ConnectDialog::onSessionComboChanged(const QString& sessionName) {
    if (sessionName != "(New Session)") {
        loadSession(sessionName);
    }
}

