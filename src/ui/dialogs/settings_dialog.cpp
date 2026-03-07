#include "settings_dialog.h"
#include <QApplication>

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
    m_categoryTree->setCurrentItem(themeItem);

    // Right: pages
    m_pages = new QStackedWidget(this);
    m_themePage = buildThemePage();
    m_pages->addWidget(m_themePage); // index 0: application theme

    // 5250 Theme page — embed the SessionSettingsDialog as a plain widget
    m_terminalThemePage = new SessionSettingsDialog(this);
    m_terminalThemePage->setWindowFlags(Qt::Widget);
    m_terminalThemePage->setWindowTitle(QString()); // not shown as dialog
    m_pages->addWidget(m_terminalThemePage); // index 1: 5250 theme

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
