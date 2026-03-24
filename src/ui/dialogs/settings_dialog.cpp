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

#include "settings_dialog.h"
#include "agent/config.h"
#include "core/macro_config.h"
#include "agent/auth/api_key_auth.h"
#include "agent/auth/oauth_auth.h"
#include "agent/auth/token_storage.h"
#include "agent/providers/anthropic_provider.h"
#include "agent/providers/openai_provider.h"
#include <QApplication>
#include <QGroupBox>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent) : ui::widgets::BaseFramelessDialog(parent) {
    setupUI();
    ensureThemesLoaded();
}

SettingsDialog::~SettingsDialog() {}

void SettingsDialog::setTerminalTheme(const ui::themes::TerminalTheme &theme) {
    if (m_terminalThemePage) {
        m_terminalThemePage->setTheme(theme);
    }
}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings");
    resize(850, 650);

    QVBoxLayout *rootLayout = contentLayout();
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left: category tree
    m_categoryTree = new QTreeWidget(this);
    m_categoryTree->setHeaderHidden(true);
    QTreeWidgetItem *themeItem = new QTreeWidgetItem(QStringList() << "Application Theme");
    m_categoryTree->addTopLevelItem(themeItem);
    QTreeWidgetItem *termThemeItem = new QTreeWidgetItem(QStringList() << "5250 Theme");
    m_categoryTree->addTopLevelItem(termThemeItem);
    QTreeWidgetItem *macrosItem = new QTreeWidgetItem(QStringList() << "Macros");
    m_categoryTree->addTopLevelItem(macrosItem);
    QTreeWidgetItem *agentItem = new QTreeWidgetItem(QStringList() << "Agents");
    m_categoryTree->addTopLevelItem(agentItem);
    m_categoryTree->setCurrentItem(themeItem);

    // Right: pages
    m_pages = new QStackedWidget(this);
    m_themePage = buildThemePage();
    m_pages->addWidget(m_themePage); // index 0: application theme

    // 5250 Theme page - embed the SessionSettingsDialog as a plain widget
    m_terminalThemePage = new SessionSettingsDialog(this);
    m_terminalThemePage->setWindowFlags(Qt::Widget);
    m_terminalThemePage->setWindowTitle(QString()); // not shown as dialog
    m_pages->addWidget(m_terminalThemePage); // index 1: 5250 theme

    // Macros page
    m_macrosPage = buildMacrosPage();
    m_pages->addWidget(m_macrosPage); // index 2: macros

    // Agent page
    m_agentPage = buildAgentPage();
    m_pages->addWidget(m_agentPage); // index 3: agent

    // Forward the embedded editor's signals
    connect(m_terminalThemePage, &SessionSettingsDialog::applyRequested,
            this, &SettingsDialog::terminalThemeApplyRequested);
    connect(m_terminalThemePage, &SessionSettingsDialog::applyToAllRequested,
            this, &SettingsDialog::terminalThemeApplyToAllRequested);

    m_splitter->addWidget(m_categoryTree);
    m_splitter->addWidget(m_pages);
    // 1/5 vs 4/5
    QList<int> sizes;
    sizes << width() / 5 << (width() * 4) / 5;
    m_splitter->setSizes(sizes);

    rootLayout->addWidget(m_splitter, 1);

    // Bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this,
            &SettingsDialog::onCloseClicked);
    btnLayout->addWidget(closeBtn);
    rootLayout->addLayout(btnLayout);

    connect(m_categoryTree, &QTreeWidget::currentItemChanged, this,
            &SettingsDialog::onCategoryChanged);
}

QWidget *SettingsDialog::buildThemePage() {
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    QLabel *label = new QLabel("Select UI Theme:", page);
    m_themeCombo = new QComboBox(page);
    m_applyThemeBtn = new QPushButton("Apply", page);
    v->addWidget(label);
    QHBoxLayout *h = new QHBoxLayout();
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    h->addWidget(m_themeCombo, 1);
    h->addWidget(m_applyThemeBtn, 0);
    v->addLayout(h);
    v->addStretch();

    // Populated by ensureThemesLoaded() which runs immediately after setupUI()
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { onThemeChanged(); });
    connect(m_applyThemeBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyThemeClicked);

    return page;
}

QWidget *SettingsDialog::buildAgentPage() {
    auto &cfg = agent::AgentConfig::instance();
    cfg.load();

    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // Provider selection
    QHBoxLayout *providerRow = new QHBoxLayout();
    providerRow->addWidget(new QLabel("Provider:", page));
    m_agentProviderCombo = new QComboBox(page);
    m_agentProviderCombo->addItem("OpenAI", "openai");
    m_agentProviderCombo->addItem("Claude (Anthropic)", "anthropic");
    providerRow->addWidget(m_agentProviderCombo, 1);
    layout->addLayout(providerRow);

    // Auth type selection
    QHBoxLayout *authTypeRow = new QHBoxLayout();
    authTypeRow->addWidget(new QLabel("Authentication:", page));
    m_agentAuthTypeCombo = new QComboBox(page);
    m_agentAuthTypeCombo->addItem("API Key", static_cast<int>(agent::AuthType::ApiKey));
    m_agentAuthTypeCombo->addItem("OAuth (Browser Sign-in)", static_cast<int>(agent::AuthType::OAuth));
    authTypeRow->addWidget(m_agentAuthTypeCombo, 1);
    layout->addLayout(authTypeRow);

    // Separator
    QFrame *sep1 = new QFrame(page);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep1);

    // Auth stacked widget
    m_authStack = new QStackedWidget(page);

    // --- Stack index 0: API Key ---
    QWidget *apiKeyPage = new QWidget(m_authStack);
    QVBoxLayout *apiKeyLayout = new QVBoxLayout(apiKeyPage);
    apiKeyLayout->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *keyRow = new QHBoxLayout();
    keyRow->addWidget(new QLabel("API Key:", apiKeyPage));
    m_agentApiKeyEdit = new QLineEdit(apiKeyPage);
    m_agentApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_agentApiKeyEdit->setPlaceholderText("Enter your API key...");
    keyRow->addWidget(m_agentApiKeyEdit, 1);
    m_agentTestBtn = new QPushButton("Test", apiKeyPage);
    keyRow->addWidget(m_agentTestBtn);
    apiKeyLayout->addLayout(keyRow);
    m_authStack->addWidget(apiKeyPage);

    // --- Stack index 1: OAuth ---
    QWidget *oauthPage = new QWidget(m_authStack);
    QVBoxLayout *oauthLayout = new QVBoxLayout(oauthPage);
    oauthLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *clientIdRow = new QHBoxLayout();
    clientIdRow->addWidget(new QLabel("Client ID:", oauthPage));
    m_oauthClientIdEdit = new QLineEdit(oauthPage);
    m_oauthClientIdEdit->setPlaceholderText("Your OAuth client ID");
    clientIdRow->addWidget(m_oauthClientIdEdit, 1);
    oauthLayout->addLayout(clientIdRow);

    QHBoxLayout *authEndpointRow = new QHBoxLayout();
    authEndpointRow->addWidget(new QLabel("Auth Endpoint:", oauthPage));
    m_oauthAuthEndpointEdit = new QLineEdit(oauthPage);
    m_oauthAuthEndpointEdit->setPlaceholderText("https://console.anthropic.com/oauth/authorize");
    authEndpointRow->addWidget(m_oauthAuthEndpointEdit, 1);
    oauthLayout->addLayout(authEndpointRow);

    QHBoxLayout *tokenEndpointRow = new QHBoxLayout();
    tokenEndpointRow->addWidget(new QLabel("Token Endpoint:", oauthPage));
    m_oauthTokenEndpointEdit = new QLineEdit(oauthPage);
    m_oauthTokenEndpointEdit->setPlaceholderText("https://console.anthropic.com/v1/oauth/token");
    tokenEndpointRow->addWidget(m_oauthTokenEndpointEdit, 1);
    oauthLayout->addLayout(tokenEndpointRow);

    QHBoxLayout *scopeRow = new QHBoxLayout();
    scopeRow->addWidget(new QLabel("Scope:", oauthPage));
    m_oauthScopeEdit = new QLineEdit(oauthPage);
    m_oauthScopeEdit->setPlaceholderText("user:inference");
    scopeRow->addWidget(m_oauthScopeEdit, 1);
    oauthLayout->addLayout(scopeRow);

    QLabel *oauthNote = new QLabel(oauthPage);
    oauthNote->setWordWrap(true);
    oauthNote->setStyleSheet("color: gray; font-size: 11px;");
    oauthNote->setText(
        "Note: Anthropic OAuth endpoints are pre-filled. "
        "A registered Client ID is required. "
        "Anthropic currently restricts OAuth to first-party applications.");
    oauthLayout->addWidget(oauthNote);

    QHBoxLayout *oauthBtnRow = new QHBoxLayout();
    m_oauthSignInBtn = new QPushButton("Sign In with Browser", oauthPage);
    m_oauthSignOutBtn = new QPushButton("Sign Out", oauthPage);
    m_oauthStatusLabel = new QLabel("Not signed in", oauthPage);
    m_oauthStatusLabel->setStyleSheet("color: gray; font-style: italic;");
    oauthBtnRow->addWidget(m_oauthSignInBtn);
    oauthBtnRow->addWidget(m_oauthSignOutBtn);
    oauthBtnRow->addWidget(m_oauthStatusLabel, 1);
    oauthLayout->addLayout(oauthBtnRow);

    m_authStack->addWidget(oauthPage);
    layout->addWidget(m_authStack);

    // Model
    QHBoxLayout *modelRow = new QHBoxLayout();
    modelRow->addWidget(new QLabel("Model:", page));
    m_agentModelCombo = new QComboBox(page);
    modelRow->addWidget(m_agentModelCombo, 1);
    layout->addLayout(modelRow);

    // Separator
    QFrame *sep2 = new QFrame(page);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep2);

    // System Prompt
    layout->addWidget(new QLabel("System Prompt:", page));
    m_agentSystemPromptEdit = new QTextEdit(page);
    m_agentSystemPromptEdit->setMaximumHeight(120);
    m_agentSystemPromptEdit->setPlainText(cfg.systemPrompt());
    layout->addWidget(m_agentSystemPromptEdit);

    // Separator
    QFrame *sep3 = new QFrame(page);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep3);

    // Auto-accept settings
    QGroupBox *dangerGroup = new QGroupBox("Danger Zone", page);
    dangerGroup->setStyleSheet(
        "QGroupBox { border: 2px solid #c62828; border-radius: 4px; margin-top: 8px; padding-top: 12px; font-weight: bold; color: #c62828; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }");
    QVBoxLayout *dangerLayout = new QVBoxLayout(dangerGroup);

    QLabel *autoAcceptWarning = new QLabel(dangerGroup);
    autoAcceptWarning->setWordWrap(true);
    autoAcceptWarning->setStyleSheet("color: #c62828; font-size: 11px; font-weight: normal;");
    autoAcceptWarning->setText(
        "WARNING: Auto-accept lets the agent act WITHOUT asking for confirmation. "
        "Scripts will run immediately on your live AS/400 session and file edits will "
        "be applied without review. This can result in data loss, unintended system "
        "changes, or destructive operations. Only enable these if you fully understand "
        "the risks and trust the agent's output.");
    dangerLayout->addWidget(autoAcceptWarning);

    m_autoAcceptAllCheck = new QCheckBox("All", dangerGroup);
    dangerLayout->addWidget(m_autoAcceptAllCheck);

    m_autoAcceptToolCallsCheck = new QCheckBox("Script execution", dangerGroup);
    m_autoAcceptToolCallsCheck->setChecked(cfg.autoAcceptToolCalls());
    dangerLayout->addWidget(m_autoAcceptToolCallsCheck);

    m_autoAcceptFileEditsCheck = new QCheckBox("File edits", dangerGroup);
    m_autoAcceptFileEditsCheck->setChecked(cfg.autoAcceptFileEdits());
    dangerLayout->addWidget(m_autoAcceptFileEditsCheck);

    // "All" toggles every individual checkbox
    connect(m_autoAcceptAllCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_autoAcceptToolCallsCheck->setChecked(checked);
        m_autoAcceptFileEditsCheck->setChecked(checked);
    });

    // Keep "All" in sync when individual checkboxes change
    auto updateAllCheck = [this]() {
        bool allChecked = m_autoAcceptToolCallsCheck->isChecked()
                       && m_autoAcceptFileEditsCheck->isChecked();
        m_autoAcceptAllCheck->blockSignals(true);
        m_autoAcceptAllCheck->setChecked(allChecked);
        m_autoAcceptAllCheck->blockSignals(false);
    };
    connect(m_autoAcceptToolCallsCheck, &QCheckBox::toggled, this, updateAllCheck);
    connect(m_autoAcceptFileEditsCheck, &QCheckBox::toggled, this, updateAllCheck);

    // Initialize "All" state
    m_autoAcceptAllCheck->setChecked(cfg.autoAcceptToolCalls() && cfg.autoAcceptFileEdits());

    layout->addWidget(dangerGroup);

    // Save button
    QHBoxLayout *saveRow = new QHBoxLayout();
    saveRow->addStretch();
    m_agentSaveBtn = new QPushButton("Save", page);
    saveRow->addWidget(m_agentSaveBtn);
    layout->addLayout(saveRow);

    layout->addStretch();

    // Populate from config
    int providerIdx = (cfg.activeProviderId() == "anthropic") ? 1 : 0;
    m_agentProviderCombo->setCurrentIndex(providerIdx);
    onAgentProviderChanged(providerIdx);

    // Connections
    connect(m_agentProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onAgentProviderChanged);
    connect(m_agentAuthTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onAgentAuthTypeChanged);
    connect(m_agentTestBtn, &QPushButton::clicked, this, &SettingsDialog::onAgentTestClicked);
    connect(m_agentSaveBtn, &QPushButton::clicked, this, &SettingsDialog::onAgentSaveClicked);
    connect(m_oauthSignInBtn, &QPushButton::clicked, this, &SettingsDialog::onOAuthSignInClicked);
    connect(m_oauthSignOutBtn, &QPushButton::clicked, this, &SettingsDialog::onOAuthSignOutClicked);

    return page;
}

QWidget *SettingsDialog::buildMacrosPage() {
    auto &cfg = core::MacroConfig::instance();
    cfg.load();

    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // Recording section
    QGroupBox *recordingGroup = new QGroupBox("Recording", page);
    QVBoxLayout *recordingLayout = new QVBoxLayout(recordingGroup);

    m_recordTimingsCheck = new QCheckBox("Record timings between keystrokes", recordingGroup);
    m_recordTimingsCheck->setChecked(cfg.recordTimings());
    m_recordTimingsCheck->setToolTip(
        "When enabled, recorded scripts will preserve the timing of each keystroke.\n"
        "When disabled (default), consecutive keystrokes are combined into a single TYPE command.");
    recordingLayout->addWidget(m_recordTimingsCheck);

    QLabel *hint = new QLabel(recordingGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray; font-size: 11px;");
    hint->setText(
        "Combined (default):  TYPE \"go main\"\n"
        "With timings:  TYPE \"g\" / WAIT 155 / TYPE \"o\" / WAIT 163 / ...");
    recordingLayout->addWidget(hint);

    layout->addWidget(recordingGroup);

    // Save button
    QHBoxLayout *saveRow = new QHBoxLayout();
    saveRow->addStretch();
    m_macrosSaveBtn = new QPushButton("Save", page);
    saveRow->addWidget(m_macrosSaveBtn);
    layout->addLayout(saveRow);

    layout->addStretch();

    connect(m_macrosSaveBtn, &QPushButton::clicked, this, &SettingsDialog::onMacrosSaveClicked);

    return page;
}

void SettingsDialog::onMacrosSaveClicked() {
    auto &cfg = core::MacroConfig::instance();
    cfg.setRecordTimings(m_recordTimingsCheck->isChecked());
    cfg.save();
    QMessageBox::information(this, "Macros Settings", "Settings saved.");
}

void SettingsDialog::onAgentProviderChanged(int index) {
    auto &cfg = agent::AgentConfig::instance();
    QString providerId = m_agentProviderCombo->itemData(index).toString();

    m_agentModelCombo->clear();
    if (providerId == "openai") {
        agent::OpenAiProvider tmp;
        m_agentModelCombo->addItems(tmp.availableModels());
        m_agentApiKeyEdit->setText(cfg.openaiApiKey());
        int modelIdx = m_agentModelCombo->findText(cfg.openaiModel());
        if (modelIdx >= 0) m_agentModelCombo->setCurrentIndex(modelIdx);

        // Set auth type
        int authIdx = (cfg.openaiAuthType() == agent::AuthType::OAuth) ? 1 : 0;
        m_agentAuthTypeCombo->setCurrentIndex(authIdx);

        // Load OAuth config
        auto oauthCfg = cfg.openaiOAuthConfig();
        m_oauthClientIdEdit->setText(oauthCfg.clientId);
        m_oauthAuthEndpointEdit->setText(oauthCfg.authorizationEndpoint);
        m_oauthTokenEndpointEdit->setText(oauthCfg.tokenEndpoint);
        m_oauthScopeEdit->setText(oauthCfg.scope);
    } else {
        agent::AnthropicProvider tmp;
        m_agentModelCombo->addItems(tmp.availableModels());
        m_agentApiKeyEdit->setText(cfg.anthropicApiKey());
        int modelIdx = m_agentModelCombo->findText(cfg.anthropicModel());
        if (modelIdx >= 0) m_agentModelCombo->setCurrentIndex(modelIdx);

        // Set auth type
        int authIdx = (cfg.anthropicAuthType() == agent::AuthType::OAuth) ? 1 : 0;
        m_agentAuthTypeCombo->setCurrentIndex(authIdx);

        // Load OAuth config
        auto oauthCfg = cfg.anthropicOAuthConfig();
        m_oauthClientIdEdit->setText(oauthCfg.clientId);
        m_oauthAuthEndpointEdit->setText(oauthCfg.authorizationEndpoint);
        m_oauthTokenEndpointEdit->setText(oauthCfg.tokenEndpoint);
        m_oauthScopeEdit->setText(oauthCfg.scope);
    }

    updateOAuthStatus();
}

void SettingsDialog::onAgentAuthTypeChanged(int index) {
    m_authStack->setCurrentIndex(index);
}

void SettingsDialog::onAgentTestClicked() {
    QString providerId = m_agentProviderCombo->currentData().toString();
    QString apiKey = m_agentApiKeyEdit->text().trimmed();
    QString model = m_agentModelCombo->currentText();

    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "Test Connection", "Please enter an API key first.");
        return;
    }

    m_agentTestBtn->setEnabled(false);
    m_agentTestBtn->setText("Testing...");

    agent::Provider *provider = nullptr;
    if (providerId == "openai") {
        provider = new agent::OpenAiProvider(this);
    } else {
        provider = new agent::AnthropicProvider(this);
    }
    provider->setApiKey(apiKey);
    provider->setModel(model);

    connect(provider, &agent::Provider::responseReceived, this, [this, provider](const QString &) {
        m_agentTestBtn->setEnabled(true);
        m_agentTestBtn->setText("Test");
        QMessageBox::information(this, "Test Connection", "Connection successful!");
        provider->deleteLater();
    });
    connect(provider, &agent::Provider::responseError, this, [this, provider](const QString &error) {
        m_agentTestBtn->setEnabled(true);
        m_agentTestBtn->setText("Test");
        QMessageBox::critical(this, "Test Connection", "Connection failed:\n" + error);
        provider->deleteLater();
    });

    provider->sendMessage("Say OK.", "Reply with only the word OK.");
}

void SettingsDialog::onAgentSaveClicked() {
    auto &cfg = agent::AgentConfig::instance();
    QString providerId = m_agentProviderCombo->currentData().toString();
    cfg.setActiveProviderId(providerId);

    auto authType = static_cast<agent::AuthType>(m_agentAuthTypeCombo->currentData().toInt());

    if (providerId == "openai") {
        cfg.setOpenaiApiKey(m_agentApiKeyEdit->text().trimmed());
        cfg.setOpenaiModel(m_agentModelCombo->currentText());
        cfg.setOpenaiAuthType(authType);
        agent::OAuthConfig oauthCfg;
        oauthCfg.clientId = m_oauthClientIdEdit->text().trimmed();
        oauthCfg.authorizationEndpoint = m_oauthAuthEndpointEdit->text().trimmed();
        oauthCfg.tokenEndpoint = m_oauthTokenEndpointEdit->text().trimmed();
        oauthCfg.scope = m_oauthScopeEdit->text().trimmed();
        oauthCfg.providerName = "OpenAI";
        cfg.setOpenaiOAuthConfig(oauthCfg);
    } else {
        cfg.setAnthropicApiKey(m_agentApiKeyEdit->text().trimmed());
        cfg.setAnthropicModel(m_agentModelCombo->currentText());
        cfg.setAnthropicAuthType(authType);
        agent::OAuthConfig oauthCfg;
        oauthCfg.clientId = m_oauthClientIdEdit->text().trimmed();
        oauthCfg.authorizationEndpoint = m_oauthAuthEndpointEdit->text().trimmed();
        oauthCfg.tokenEndpoint = m_oauthTokenEndpointEdit->text().trimmed();
        oauthCfg.scope = m_oauthScopeEdit->text().trimmed();
        oauthCfg.providerName = "Claude (Anthropic)";
        cfg.setAnthropicOAuthConfig(oauthCfg);
    }
    cfg.setSystemPrompt(m_agentSystemPromptEdit->toPlainText());

    // Handle auto-accept with confirmation when enabling any option
    bool wantToolCalls = m_autoAcceptToolCallsCheck->isChecked();
    bool wantFileEdits = m_autoAcceptFileEditsCheck->isChecked();
    bool enablingAny = (wantToolCalls && !cfg.autoAcceptToolCalls())
                    || (wantFileEdits && !cfg.autoAcceptFileEdits());

    if (enablingAny) {
        QMessageBox warn(this);
        warn.setWindowTitle("Enable Auto-Accept");
        warn.setIcon(QMessageBox::Warning);
        warn.setText("Are you sure you want to enable auto-accept?");
        warn.setInformativeText(
            "The agent will perform the selected actions on your live AS/400 "
            "session without asking for confirmation. This can result in data "
            "loss or unintended system changes.\n\n"
            "Do you want to proceed?");
        warn.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        warn.setDefaultButton(QMessageBox::No);
        if (warn.exec() != QMessageBox::Yes) {
            m_autoAcceptToolCallsCheck->setChecked(cfg.autoAcceptToolCalls());
            m_autoAcceptFileEditsCheck->setChecked(cfg.autoAcceptFileEdits());
            wantToolCalls = cfg.autoAcceptToolCalls();
            wantFileEdits = cfg.autoAcceptFileEdits();
        }
    }
    cfg.setAutoAcceptToolCalls(wantToolCalls);
    cfg.setAutoAcceptFileEdits(wantFileEdits);

    cfg.save();
    emit agentConfigChanged();

    QMessageBox::information(this, "Agent Settings", "Settings saved.");
}

void SettingsDialog::onOAuthSignInClicked() {
    QString clientId = m_oauthClientIdEdit->text().trimmed();
    QString authEndpoint = m_oauthAuthEndpointEdit->text().trimmed();
    QString tokenEndpoint = m_oauthTokenEndpointEdit->text().trimmed();

    if (clientId.isEmpty() || authEndpoint.isEmpty() || tokenEndpoint.isEmpty()) {
        QMessageBox::warning(this, "OAuth Sign In",
                             "Please fill in the Client ID, Auth Endpoint, and Token Endpoint.");
        return;
    }

    agent::OAuthConfig oauthCfg;
    oauthCfg.clientId = clientId;
    oauthCfg.authorizationEndpoint = authEndpoint;
    oauthCfg.tokenEndpoint = tokenEndpoint;
    oauthCfg.scope = m_oauthScopeEdit->text().trimmed();

    QString providerId = m_agentProviderCombo->currentData().toString();

    if (m_activeOAuth) {
        m_activeOAuth->deleteLater();
    }
    m_activeOAuth = new agent::OAuthAuth(oauthCfg, providerId, this);

    m_oauthSignInBtn->setEnabled(false);
    m_oauthSignInBtn->setText("Waiting for browser...");
    m_oauthStatusLabel->setText("Opening browser...");

    connect(m_activeOAuth, &agent::AuthMethod::authenticationSucceeded, this, [this]() {
        m_oauthSignInBtn->setEnabled(true);
        m_oauthSignInBtn->setText("Sign In with Browser");
        updateOAuthStatus();
        QMessageBox::information(this, "OAuth", "Successfully signed in!");
    });
    connect(m_activeOAuth, &agent::AuthMethod::authenticationFailed, this, [this](const QString &error) {
        m_oauthSignInBtn->setEnabled(true);
        m_oauthSignInBtn->setText("Sign In with Browser");
        m_oauthStatusLabel->setText("Not signed in");
        m_oauthStatusLabel->setStyleSheet("color: red; font-style: italic;");
        QMessageBox::critical(this, "OAuth", "Sign in failed:\n" + error);
    });

    m_activeOAuth->authenticate();
}

void SettingsDialog::onOAuthSignOutClicked() {
    QString providerId = m_agentProviderCombo->currentData().toString();
    agent::TokenStorage::instance().clearTokens(providerId);
    if (m_activeOAuth) {
        m_activeOAuth->logout();
        m_activeOAuth->deleteLater();
        m_activeOAuth = nullptr;
    }
    updateOAuthStatus();
    QMessageBox::information(this, "OAuth", "Signed out.");
}

void SettingsDialog::updateOAuthStatus() {
    QString providerId = m_agentProviderCombo->currentData().toString();
    QString accessToken, refreshToken;
    QDateTime expiresAt;
    if (agent::TokenStorage::instance().loadTokens(providerId, accessToken, refreshToken, expiresAt)) {
        if (QDateTime::currentDateTimeUtc() < expiresAt) {
            m_oauthStatusLabel->setText("Signed in (expires " +
                                        expiresAt.toLocalTime().toString("yyyy-MM-dd hh:mm") + ")");
            m_oauthStatusLabel->setStyleSheet("color: green; font-style: italic;");
        } else {
            m_oauthStatusLabel->setText("Token expired - please sign in again");
            m_oauthStatusLabel->setStyleSheet("color: orange; font-style: italic;");
        }
    } else {
        m_oauthStatusLabel->setText("Not signed in");
        m_oauthStatusLabel->setStyleSheet("color: gray; font-style: italic;");
    }
}

void SettingsDialog::ensureThemesLoaded() {
    auto &mgr = ui::themes::ThemeManager::instance();
    if (mgr.availableThemes().isEmpty()) {
        mgr.loadBuiltinThemes();
    }
    // Populate combo: visible text = displayName, user data = internal name
    m_themeCombo->blockSignals(true);
    m_themeCombo->clear();
    const QString current = mgr.currentThemeName();
    int selectIdx = 0;
    for (const QString &name : mgr.availableThemes()) {
        m_themeCombo->addItem(mgr.theme(name).displayName, name);
        if (name == current) {
            selectIdx = m_themeCombo->count() - 1;
        }
    }
    m_themeCombo->setCurrentIndex(selectIdx);
    m_themeCombo->blockSignals(false);
}

void SettingsDialog::onCategoryChanged(QTreeWidgetItem *current,
                                       QTreeWidgetItem *previous) {
    Q_UNUSED(previous);
    if (!current)
        return;
    if (current->text(0) == "Application Theme") {
        m_pages->setCurrentWidget(m_themePage);
    } else if (current->text(0) == "5250 Theme") {
        m_pages->setCurrentWidget(m_terminalThemePage);
    } else if (current->text(0) == "Macros") {
        m_pages->setCurrentWidget(m_macrosPage);
    } else if (current->text(0) == "Agents") {
        m_pages->setCurrentWidget(m_agentPage);
    }
}

void SettingsDialog::onThemeChanged() {
    const QString name = m_themeCombo->currentData().toString();
    if (name.isEmpty())
        return;
    auto &mgr = ui::themes::ThemeManager::instance();
    if (!mgr.hasTheme(name))
        return;
    mgr.setCurrentTheme(name);
}

void SettingsDialog::onApplyThemeClicked() {
    const QString name = m_themeCombo->currentData().toString();
    if (name.isEmpty())
        return;
    auto &mgr = ui::themes::ThemeManager::instance();
    if (!mgr.hasTheme(name))
        return;
    mgr.setCurrentTheme(name);
}

void SettingsDialog::onCloseClicked() { accept(); }
