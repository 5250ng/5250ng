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
#include "agent/tool_definitions.h"
#include "core/macro_config.h"
#include "agent/auth/api_key_auth.h"
#include "agent/auth/oauth_auth.h"
#include "agent/auth/token_storage.h"
#include "agent/providers/anthropic_provider.h"
#include "agent/providers/openai_provider.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QStandardPaths>
#include <QUrl>
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>

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
    QTreeWidgetItem *themesCategory = new QTreeWidgetItem(QStringList() << "Themes");
    m_categoryTree->addTopLevelItem(themesCategory);
    QTreeWidgetItem *themeItem = new QTreeWidgetItem(themesCategory, QStringList() << "Application Theme");
    QTreeWidgetItem *termThemeItem = new QTreeWidgetItem(themesCategory, QStringList() << "5250 Theme");
    themesCategory->setExpanded(true);
    // Scripts category: General + Recording
    QTreeWidgetItem *scriptsCategory = new QTreeWidgetItem(QStringList() << "Scripts");
    m_categoryTree->addTopLevelItem(scriptsCategory);
    new QTreeWidgetItem(scriptsCategory, QStringList() << "General");
    new QTreeWidgetItem(scriptsCategory, QStringList() << "Recording");
    scriptsCategory->setExpanded(true);

    // AI category: Agent Panel + MCP Server
    QTreeWidgetItem *aiCategory = new QTreeWidgetItem(QStringList() << "AI");
    m_categoryTree->addTopLevelItem(aiCategory);
    new QTreeWidgetItem(aiCategory, QStringList() << "Agent Panel");
    QTreeWidgetItem *mcpItem = new QTreeWidgetItem(aiCategory, QStringList() << "MCP Server");
    new QTreeWidgetItem(mcpItem, QStringList() << "General");
    new QTreeWidgetItem(mcpItem, QStringList() << "Tools");
    aiCategory->setExpanded(true);
    mcpItem->setExpanded(true);
    m_categoryTree->setCurrentItem(themeItem);

    m_toolNames = {
        agent::kToolConnect, agent::kToolGenerateScript,
        agent::kToolGetCursorPosition, agent::kToolGetFieldAt,
        agent::kToolListFiles, agent::kToolLogin,
        agent::kToolReadFile, agent::kToolReadScreen,
        agent::kToolRunScript, agent::kToolSendKeys,
        agent::kToolWriteFile};
    m_toolNames.sort();

    // Right: pages wrapped in a frame to match the category tree style
    m_pages = new QStackedWidget(this);
    m_themePage = buildThemePage();
    m_pages->addWidget(m_themePage); // index 0: application theme

    // 5250 Theme page - embed the SessionSettingsDialog as a plain widget
    m_terminalThemePage = new SessionSettingsDialog(this);
    m_terminalThemePage->setWindowFlags(Qt::Widget);
    m_terminalThemePage->setWindowTitle(QString()); // not shown as dialog
    m_terminalThemePage->setEmbeddedMode(true);
    m_pages->addWidget(m_terminalThemePage); // index 1: 5250 theme

    // Scripts General page
    m_scriptsGeneralPage = buildScriptsGeneralPage();
    m_pages->addWidget(m_scriptsGeneralPage); // index 2: scripts general

    // Scripts Recording page (formerly Macros)
    m_macrosPage = buildMacrosPage();
    m_pages->addWidget(m_macrosPage); // index 3: recording

    // MCP Server page
    m_mcpPage = buildMcpServerPage();
    m_pages->addWidget(m_mcpPage); // index 3: MCP server

    // Agent page
    m_agentPage = buildAgentPage();
    m_pages->addWidget(m_agentPage); // index 4: agent

    // Tools page (unified table view)
    m_toolsPage = buildToolsPage();
    m_pages->addWidget(m_toolsPage); // index 5: tools

    // Forward the embedded editor's signals
    connect(m_terminalThemePage, &SessionSettingsDialog::applyRequested,
            this, &SettingsDialog::terminalThemeApplyRequested);
    connect(m_terminalThemePage, &SessionSettingsDialog::applyToAllRequested,
            this, &SettingsDialog::terminalThemeApplyToAllRequested);

    QFrame *pagesFrame = new QFrame(this);
    pagesFrame->setFrameShape(QFrame::StyledPanel);
    pagesFrame->setFrameShadow(QFrame::Sunken);
    QVBoxLayout *pagesFrameLayout = new QVBoxLayout(pagesFrame);
    pagesFrameLayout->setContentsMargins(0, 0, 0, 0);
    pagesFrameLayout->addWidget(m_pages);

    m_splitter->addWidget(m_categoryTree);
    m_splitter->addWidget(pagesFrame);
    // 1/5 vs 4/5
    QList<int> sizes;
    sizes << width() / 5 << (width() * 4) / 5;
    m_splitter->setSizes(sizes);

    rootLayout->addWidget(m_splitter, 1);

    // Bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_saveBtn = new QPushButton("Save", this);
    connect(m_saveBtn, &QPushButton::clicked, this,
            &SettingsDialog::onSaveClicked);
    btnLayout->addWidget(m_saveBtn);
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
    v->addWidget(label);
    v->addWidget(m_themeCombo);
    v->addStretch();

    // Populated by ensureThemesLoaded() which runs immediately after setupUI()
    // Theme is applied only when the user clicks Save (via onSaveClicked)

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
    connect(m_oauthSignInBtn, &QPushButton::clicked, this, &SettingsDialog::onOAuthSignInClicked);
    connect(m_oauthSignOutBtn, &QPushButton::clicked, this, &SettingsDialog::onOAuthSignOutClicked);

    return page;
}

static QString defaultDescriptionForTool(const QString &name) {
    if (name == agent::kToolConnect)           return agent::kToolConnectDescription;
    if (name == agent::kToolGenerateScript)    return agent::kToolGenerateScriptDescription;
    if (name == agent::kToolGetCursorPosition) return agent::kToolGetCursorPositionDescription;
    if (name == agent::kToolGetFieldAt)        return agent::kToolGetFieldAtDescription;
    if (name == agent::kToolListFiles)         return agent::kToolListFilesDescription;
    if (name == agent::kToolLogin)            return agent::kToolLoginDescription;
    if (name == agent::kToolReadFile)          return agent::kToolReadFileDescription;
    if (name == agent::kToolReadScreen)        return agent::kToolReadScreenDescription;
    if (name == agent::kToolRunScript)         return agent::kToolRunScriptDescription;
    if (name == agent::kToolSendKeys)          return agent::kToolSendKeysDescription;
    if (name == agent::kToolWriteFile)         return agent::kToolWriteFileDescription;
    return {};
}

static QJsonObject schemaForTool(const QString &name) {
    if (name == agent::kToolConnect)           return agent::toolConnectSchema();
    if (name == agent::kToolGenerateScript)    return agent::toolGenerateScriptSchema();
    if (name == agent::kToolGetCursorPosition) return agent::toolGetCursorPositionSchema();
    if (name == agent::kToolGetFieldAt)        return agent::toolGetFieldAtSchema();
    if (name == agent::kToolListFiles)         return agent::toolListFilesSchema();
    if (name == agent::kToolLogin)            return agent::toolLoginSchema();
    if (name == agent::kToolReadFile)          return agent::toolReadFileSchema();
    if (name == agent::kToolReadScreen)        return agent::toolReadScreenSchema();
    if (name == agent::kToolRunScript)         return agent::toolRunScriptSchema();
    if (name == agent::kToolSendKeys)          return agent::toolSendKeysSchema();
    if (name == agent::kToolWriteFile)         return agent::toolWriteFileSchema();
    return {};
}

QWidget *SettingsDialog::buildToolsPage() {
    auto &cfg = agent::AgentConfig::instance();

    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    // Toolbar
    QHBoxLayout *toolbar = new QHBoxLayout();
    QPushButton *enableAllBtn = new QPushButton("Enable All", page);
    QPushButton *disableAllBtn = new QPushButton("Disable All", page);
    toolbar->addWidget(enableAllBtn);
    toolbar->addWidget(disableAllBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    connect(enableAllBtn, &QPushButton::clicked, this, [this]() {
        for (int r = 0; r < m_toolsTable->rowCount(); ++r)
            m_toolsTable->item(r, 0)->setCheckState(Qt::Checked);
    });
    connect(disableAllBtn, &QPushButton::clicked, this, [this]() {
        for (int r = 0; r < m_toolsTable->rowCount(); ++r)
            m_toolsTable->item(r, 0)->setCheckState(Qt::Unchecked);
    });

    // Table
    m_toolsTable = new QTableWidget(m_toolNames.size(), 4, page);
    m_toolsTable->setHorizontalHeaderLabels({"", "Tool", "Description", "Parameters"});
    m_toolsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_toolsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_toolsTable->verticalHeader()->setVisible(false);
    m_toolsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toolsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_toolsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int i = 0; i < m_toolNames.size(); ++i) {
        const QString &name = m_toolNames[i];
        agent::ToolConfig tc = cfg.toolConfig(name);
        QString defaultDesc = defaultDescriptionForTool(name);
        QString displayDesc = tc.customDescription.isEmpty() ? defaultDesc : tc.customDescription;

        // Enabled checkbox item
        auto *checkItem = new QTableWidgetItem();
        checkItem->setCheckState(tc.enabled ? Qt::Checked : Qt::Unchecked);
        m_toolsTable->setItem(i, 0, checkItem);

        // Tool name (bold)
        auto *nameItem = new QTableWidgetItem(name);
        QFont boldFont = nameItem->font();
        boldFont.setBold(true);
        nameItem->setFont(boldFont);
        m_toolsTable->setItem(i, 1, nameItem);

        // Description (truncated, gray italic if default)
        auto *descItem = new QTableWidgetItem(displayDesc);
        if (tc.customDescription.isEmpty()) {
            QFont italicFont = descItem->font();
            italicFont.setItalic(true);
            descItem->setFont(italicFont);
            descItem->setForeground(QColor(128, 128, 128));
        }
        descItem->setToolTip(displayDesc);
        m_toolsTable->setItem(i, 2, descItem);

        // Parameters summary
        QJsonObject schema = schemaForTool(name);
        QJsonObject props = schema["properties"].toObject();
        QJsonArray required = schema["required"].toArray();
        QStringList paramList;
        for (auto it = props.begin(); it != props.end(); ++it) {
            QString p = it.key();
            if (required.contains(p)) p += "*";
            paramList << p;
        }
        auto *paramsItem = new QTableWidgetItem(
            paramList.isEmpty() ? "(none)" : paramList.join(", "));
        if (paramList.isEmpty())
            paramsItem->setForeground(QColor(128, 128, 128));
        m_toolsTable->setItem(i, 3, paramsItem);
    }

    layout->addWidget(m_toolsTable, 1);

    // Detail panel
    QGroupBox *detailGroup = new QGroupBox("Tool Details", page);
    QVBoxLayout *detailLayout = new QVBoxLayout(detailGroup);

    m_toolDetailDefaultDesc = new QLabel(page);
    m_toolDetailDefaultDesc->setWordWrap(true);
    m_toolDetailDefaultDesc->setStyleSheet("color: gray;");
    detailLayout->addWidget(new QLabel("<b>Default Description:</b>", page));
    detailLayout->addWidget(m_toolDetailDefaultDesc);

    detailLayout->addWidget(new QLabel("<b>Custom Description</b> (overrides default when set):", page));
    m_toolDetailCustomDesc = new QTextEdit(page);
    m_toolDetailCustomDesc->setMaximumHeight(80);
    m_toolDetailCustomDesc->setPlaceholderText("Leave empty to use default description");
    detailLayout->addWidget(m_toolDetailCustomDesc);

    m_toolDetailParams = new QLabel(page);
    m_toolDetailParams->setWordWrap(true);
    m_toolDetailParams->setTextFormat(Qt::RichText);
    detailLayout->addWidget(new QLabel("<b>Parameters:</b>", page));
    detailLayout->addWidget(m_toolDetailParams);

    layout->addWidget(detailGroup);

    connect(m_toolsTable, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) { onToolsTableSelectionChanged(); });

    // Select first row
    if (m_toolsTable->rowCount() > 0)
        m_toolsTable->selectRow(0);
    onToolsTableSelectionChanged();

    return page;
}

void SettingsDialog::onToolsTableSelectionChanged() {
    int row = m_toolsTable->currentRow();
    if (row < 0 || row >= m_toolNames.size()) {
        m_toolDetailDefaultDesc->clear();
        m_toolDetailCustomDesc->clear();
        m_toolDetailParams->clear();
        return;
    }

    const QString &name = m_toolNames[row];
    auto &cfg = agent::AgentConfig::instance();
    agent::ToolConfig tc = cfg.toolConfig(name);

    m_toolDetailDefaultDesc->setText(defaultDescriptionForTool(name));
    m_toolDetailCustomDesc->setPlainText(tc.customDescription);
    m_toolDetailCustomDesc->setPlaceholderText(defaultDescriptionForTool(name));

    // Build parameters detail
    QJsonObject schema = schemaForTool(name);
    QJsonObject props = schema["properties"].toObject();
    QJsonArray required = schema["required"].toArray();
    if (props.isEmpty()) {
        m_toolDetailParams->setText("<i>No parameters</i>");
    } else {
        QStringList lines;
        for (auto it = props.begin(); it != props.end(); ++it) {
            QJsonObject prop = it.value().toObject();
            bool isReq = required.contains(it.key());
            lines << QString("<b>%1</b> (%2)%3 — %4")
                .arg(it.key(), prop["type"].toString(),
                     isReq ? " <i>required</i>" : "",
                     prop["description"].toString());
        }
        m_toolDetailParams->setText(lines.join("<br>"));
    }
}

QWidget *SettingsDialog::buildScriptsGeneralPage() {
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // Scripts directory (read-only display)
    QGroupBox *dirGroup = new QGroupBox("Scripts Directory", page);
    QHBoxLayout *dirLayout = new QHBoxLayout(dirGroup);
    m_scriptsDirEdit = new QLineEdit(dirGroup);
    QString scriptsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + "/scripts";
    m_scriptsDirEdit->setText(QDir::toNativeSeparators(scriptsPath));
    m_scriptsDirEdit->setReadOnly(true);
    m_scriptsDirEdit->setToolTip("Scripts are stored in this directory.\n"
                                  "Use Scripts > Open Scripts Folder to browse.");
    dirLayout->addWidget(m_scriptsDirEdit, 1);
    QPushButton *openDirBtn = new QPushButton("Open", dirGroup);
    connect(openDirBtn, &QPushButton::clicked, this, [scriptsPath]() {
        QDir().mkpath(scriptsPath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath));
    });
    dirLayout->addWidget(openDirBtn);
    layout->addWidget(dirGroup);

    // Default execution settings
    QGroupBox *execGroup = new QGroupBox("Default Execution Settings", page);
    QFormLayout *execLayout = new QFormLayout(execGroup);

    m_defaultTimeoutSpin = new QSpinBox(execGroup);
    m_defaultTimeoutSpin->setRange(1000, 120000);
    m_defaultTimeoutSpin->setSingleStep(1000);
    m_defaultTimeoutSpin->setSuffix(" ms");
    m_defaultTimeoutSpin->setValue(30000);
    m_defaultTimeoutSpin->setToolTip(
        "Default timeout for EXPECT commands.\n"
        "Scripts can override this with GLOBAL EXPECT_TIMEOUT.");
    execLayout->addRow("EXPECT timeout:", m_defaultTimeoutSpin);

    m_defaultDelaySpin = new QSpinBox(execGroup);
    m_defaultDelaySpin->setRange(0, 5000);
    m_defaultDelaySpin->setSingleStep(50);
    m_defaultDelaySpin->setSuffix(" ms");
    m_defaultDelaySpin->setValue(0);
    m_defaultDelaySpin->setToolTip(
        "Fixed delay between script actions.\n"
        "Scripts can override this with GLOBAL DELAY.");
    execLayout->addRow("Action delay:", m_defaultDelaySpin);

    QHBoxLayout *jitterRow = new QHBoxLayout();
    m_defaultJitterMinSpin = new QSpinBox(execGroup);
    m_defaultJitterMinSpin->setRange(0, 5000);
    m_defaultJitterMinSpin->setSingleStep(50);
    m_defaultJitterMinSpin->setSuffix(" ms");
    m_defaultJitterMinSpin->setValue(0);
    m_defaultJitterMaxSpin = new QSpinBox(execGroup);
    m_defaultJitterMaxSpin->setRange(0, 5000);
    m_defaultJitterMaxSpin->setSingleStep(50);
    m_defaultJitterMaxSpin->setSuffix(" ms");
    m_defaultJitterMaxSpin->setValue(0);
    jitterRow->addWidget(m_defaultJitterMinSpin);
    jitterRow->addWidget(new QLabel("to", execGroup));
    jitterRow->addWidget(m_defaultJitterMaxSpin);
    jitterRow->addStretch();
    execLayout->addRow("Jitter range:", jitterRow);

    QLabel *execHint = new QLabel(execGroup);
    execHint->setWordWrap(true);
    execHint->setStyleSheet("color: gray; font-size: 11px;");
    execHint->setText("These defaults apply to all scripts. Individual scripts can override "
                      "them with GLOBAL EXPECT_TIMEOUT, GLOBAL DELAY, and GLOBAL JITTER commands.");
    execLayout->addRow(execHint);

    layout->addWidget(execGroup);

    // Behavior
    QGroupBox *behaviorGroup = new QGroupBox("Behavior", page);
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);
    m_confirmDeleteCheck = new QCheckBox("Confirm before deleting scripts", behaviorGroup);
    m_confirmDeleteCheck->setChecked(true);
    behaviorLayout->addWidget(m_confirmDeleteCheck);
    layout->addWidget(behaviorGroup);

    layout->addStretch();
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

    layout->addStretch();

    return page;
}

void SettingsDialog::onMacrosSaveClicked() {
    auto &cfg = core::MacroConfig::instance();
    cfg.setRecordTimings(m_recordTimingsCheck->isChecked());
    cfg.save();
    ui::widgets::StyledMessageBox::information(this, "Macros Settings", "Settings saved.");
}

QWidget *SettingsDialog::buildMcpServerPage() {
    auto &cfg = agent::AgentConfig::instance();
    cfg.load();

    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // Auto-start section
    QGroupBox *startupGroup = new QGroupBox("Startup", page);
    QVBoxLayout *startupLayout = new QVBoxLayout(startupGroup);

    m_mcpAutoStartCheck = new QCheckBox("Automatically start MCP server on launch", startupGroup);
    m_mcpAutoStartCheck->setChecked(cfg.mcpAutoStart());
    m_mcpAutoStartCheck->setToolTip("When enabled, the MCP server will start automatically each time the application launches.");
    startupLayout->addWidget(m_mcpAutoStartCheck);

    layout->addWidget(startupGroup);

    // Network section
    QGroupBox *networkGroup = new QGroupBox("Network", page);
    QVBoxLayout *networkLayout = new QVBoxLayout(networkGroup);

    QHBoxLayout *portRow = new QHBoxLayout();
    portRow->addWidget(new QLabel("Port:", networkGroup));
    m_mcpPortSpin = new QSpinBox(networkGroup);
    m_mcpPortSpin->setRange(1024, 65535);
    m_mcpPortSpin->setValue(cfg.mcpServerPort());
    portRow->addWidget(m_mcpPortSpin);
    portRow->addStretch();
    networkLayout->addLayout(portRow);

    QLabel *hint = new QLabel(networkGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray; font-size: 11px;");
    hint->setText("MCP clients connect via HTTP POST to http://localhost:<port>/mcp\n"
                  "The server can also be started for a single session with the --enable-mcp-server flag.");
    networkLayout->addWidget(hint);

    layout->addWidget(networkGroup);

    layout->addStretch();
    return page;
}

void SettingsDialog::onMcpSaveClicked() {
    auto &cfg = agent::AgentConfig::instance();
    cfg.setMcpAutoStart(m_mcpAutoStartCheck->isChecked());
    cfg.setMcpServerPort(static_cast<quint16>(m_mcpPortSpin->value()));
    cfg.save();
    ui::widgets::StyledMessageBox::information(this, "MCP Server Settings", "Settings saved.");
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
        ui::widgets::StyledMessageBox::warning(this, "Test Connection", "Please enter an API key first.");
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
        ui::widgets::StyledMessageBox::information(this, "Test Connection", "Connection successful!");
        provider->deleteLater();
    });
    connect(provider, &agent::Provider::responseError, this, [this, provider](const QString &error) {
        m_agentTestBtn->setEnabled(true);
        m_agentTestBtn->setText("Test");
        ui::widgets::StyledMessageBox::warning(this, "Test Connection", "Connection failed:\n" + error);
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
        auto result = ui::widgets::StyledMessageBox::question(this, "Enable Auto-Accept",
            "Are you sure you want to enable auto-accept?\n\n"
            "The agent will perform the selected actions on your live AS/400 "
            "session without asking for confirmation. This can result in data "
            "loss or unintended system changes.\n\n"
            "Do you want to proceed?");
        if (result != ui::widgets::StyledMessageBox::Yes) {
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

    ui::widgets::StyledMessageBox::information(this, "Agent Settings", "Settings saved.");
}

void SettingsDialog::onOAuthSignInClicked() {
    QString clientId = m_oauthClientIdEdit->text().trimmed();
    QString authEndpoint = m_oauthAuthEndpointEdit->text().trimmed();
    QString tokenEndpoint = m_oauthTokenEndpointEdit->text().trimmed();

    if (clientId.isEmpty() || authEndpoint.isEmpty() || tokenEndpoint.isEmpty()) {
        ui::widgets::StyledMessageBox::warning(this, "OAuth Sign In",
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
        ui::widgets::StyledMessageBox::information(this, "OAuth", "Successfully signed in!");
    });
    connect(m_activeOAuth, &agent::AuthMethod::authenticationFailed, this, [this](const QString &error) {
        m_oauthSignInBtn->setEnabled(true);
        m_oauthSignInBtn->setText("Sign In with Browser");
        m_oauthStatusLabel->setText("Not signed in");
        m_oauthStatusLabel->setStyleSheet("color: red; font-style: italic;");
        ui::widgets::StyledMessageBox::warning(this, "OAuth", "Sign in failed:\n" + error);
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
    ui::widgets::StyledMessageBox::information(this, "OAuth", "Signed out.");
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
    QString text = current->text(0);
    if (text == "Themes" || text == "Application Theme") {
        m_pages->setCurrentWidget(m_themePage);
    } else if (text == "5250 Theme") {
        m_pages->setCurrentWidget(m_terminalThemePage);
    } else if (text == "Scripts" || text == "General") {
        // "General" could be Scripts > General or MCP Server > General
        // Check parent to disambiguate
        if (current->parent() && current->parent()->text(0) == "MCP Server")
            m_pages->setCurrentWidget(m_mcpPage);
        else
            m_pages->setCurrentWidget(m_scriptsGeneralPage);
    } else if (text == "Recording") {
        m_pages->setCurrentWidget(m_macrosPage);
    } else if (text == "AI" || text == "Agent Panel") {
        m_pages->setCurrentWidget(m_agentPage);
    } else if (text == "MCP Server") {
        m_pages->setCurrentWidget(m_mcpPage);
    } else if (text == "Tools") {
        m_pages->setCurrentWidget(m_toolsPage);
    }
}

void SettingsDialog::onSaveClicked() {
    QWidget *currentPage = m_pages->currentWidget();

    if (currentPage == m_toolsPage) {
        // Save all tools from the table + detail panel
        auto &cfg = agent::AgentConfig::instance();
        int selectedRow = m_toolsTable->currentRow();
        for (int i = 0; i < m_toolNames.size(); ++i) {
            agent::ToolConfig tc;
            tc.enabled = (m_toolsTable->item(i, 0)->checkState() == Qt::Checked);
            // If this row is selected, read custom desc from the detail editor
            if (i == selectedRow) {
                tc.customDescription = m_toolDetailCustomDesc->toPlainText().trimmed();
            } else {
                tc.customDescription = cfg.toolConfig(m_toolNames[i]).customDescription;
            }
            cfg.setToolConfig(m_toolNames[i], tc);
        }
        cfg.save();
        emit agentConfigChanged();
        ui::widgets::StyledMessageBox::information(this, "Tools", "Tool settings saved.");
        return;
    }

    if (currentPage == m_themePage) {
        const QString name = m_themeCombo->currentData().toString();
        if (!name.isEmpty()) {
            auto &mgr = ui::themes::ThemeManager::instance();
            if (mgr.hasTheme(name))
                mgr.setCurrentTheme(name);
        }
    } else if (currentPage == m_terminalThemePage) {
        m_terminalThemePage->onApplyToAll();
    } else if (currentPage == m_macrosPage) {
        onMacrosSaveClicked();
    } else if (currentPage == m_mcpPage) {
        onMcpSaveClicked();
    } else if (currentPage == m_agentPage) {
        onAgentSaveClicked();
    }
}

// onToolSaveClicked removed — tools are saved in bulk from the unified table

void SettingsDialog::onCloseClicked() { accept(); }
