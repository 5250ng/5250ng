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

    // Agent page
    m_agentPage = buildAgentPage();
    m_pages->addWidget(m_agentPage); // index 2: agent

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

    // Separator
    QFrame *sep1 = new QFrame(page);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep1);

    // API Key
    QHBoxLayout *keyRow = new QHBoxLayout();
    keyRow->addWidget(new QLabel("API Key:", page));
    m_agentApiKeyEdit = new QLineEdit(page);
    m_agentApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_agentApiKeyEdit->setPlaceholderText("Enter your API key...");
    keyRow->addWidget(m_agentApiKeyEdit, 1);
    m_agentTestBtn = new QPushButton("Test", page);
    keyRow->addWidget(m_agentTestBtn);
    layout->addLayout(keyRow);

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
    connect(m_agentTestBtn, &QPushButton::clicked, this, &SettingsDialog::onAgentTestClicked);
    connect(m_agentSaveBtn, &QPushButton::clicked, this, &SettingsDialog::onAgentSaveClicked);

    return page;
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
    } else {
        agent::AnthropicProvider tmp;
        m_agentModelCombo->addItems(tmp.availableModels());
        m_agentApiKeyEdit->setText(cfg.anthropicApiKey());
        int modelIdx = m_agentModelCombo->findText(cfg.anthropicModel());
        if (modelIdx >= 0) m_agentModelCombo->setCurrentIndex(modelIdx);
    }
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

    if (providerId == "openai") {
        cfg.setOpenaiApiKey(m_agentApiKeyEdit->text().trimmed());
        cfg.setOpenaiModel(m_agentModelCombo->currentText());
    } else {
        cfg.setAnthropicApiKey(m_agentApiKeyEdit->text().trimmed());
        cfg.setAnthropicModel(m_agentModelCombo->currentText());
    }
    cfg.setSystemPrompt(m_agentSystemPromptEdit->toPlainText());
    cfg.save();

    QMessageBox::information(this, "Agent Settings", "Settings saved.");
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
