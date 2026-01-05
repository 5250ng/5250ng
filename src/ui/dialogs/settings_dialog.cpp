#include "settings_dialog.h"
#include <QApplication>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setupUI();
    ensureThemesLoaded();
}

SettingsDialog::~SettingsDialog() {}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings");
    resize(800, 500);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left: category tree
    m_categoryTree = new QTreeWidget(this);
    m_categoryTree->setHeaderHidden(true);
    QTreeWidgetItem *themeItem = new QTreeWidgetItem(QStringList() << "Theme");
    m_categoryTree->addTopLevelItem(themeItem);
    m_categoryTree->setCurrentItem(themeItem);

    // Right: pages
    m_pages = new QStackedWidget(this);
    m_themePage = buildThemePage();
    m_pages->addWidget(m_themePage); // index 0: theme

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

    // Populate themes
    QStringList themes = ui::themes::ThemeManager::instance().availableThemes();
    if (themes.isEmpty()) {
        // Will get loaded by ensureThemesLoaded; keep placeholder for now
    } else {
        m_themeCombo->addItems(themes);
        QString current = ui::themes::ThemeManager::instance().currentThemeName();
        int idx = m_themeCombo->findText(current);
        if (idx >= 0)
            m_themeCombo->setCurrentIndex(idx);
    }
    connect(m_themeCombo, &QComboBox::currentTextChanged, this,
            &SettingsDialog::onThemeChanged);
    connect(m_applyThemeBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyThemeClicked);

    return page;
}

void SettingsDialog::ensureThemesLoaded() {
    auto &mgr = ui::themes::ThemeManager::instance();
    if (mgr.availableThemes().isEmpty()) {
        mgr.loadBuiltinThemes();
    }
    // refresh combo
    m_themeCombo->blockSignals(true);
    m_themeCombo->clear();
    m_themeCombo->addItems(mgr.availableThemes());
    int idx = m_themeCombo->findText(mgr.currentThemeName());
    if (idx >= 0)
        m_themeCombo->setCurrentIndex(idx);
    m_themeCombo->blockSignals(false);
}

void SettingsDialog::onCategoryChanged(QTreeWidgetItem *current,
                                       QTreeWidgetItem *previous) {
    Q_UNUSED(previous);
    if (!current)
        return;
    if (current->text(0) == "Theme") {
        m_pages->setCurrentWidget(m_themePage);
    }
}

void SettingsDialog::onThemeChanged(const QString &themeName) {
    if (themeName.isEmpty())
        return;
    auto &mgr = ui::themes::ThemeManager::instance();
    if (!mgr.hasTheme(themeName))
        return;
    mgr.setCurrentTheme(themeName);
}

void SettingsDialog::onApplyThemeClicked() {
    const QString themeName = m_themeCombo->currentText();
    if (themeName.isEmpty())
        return;
    auto &mgr = ui::themes::ThemeManager::instance();
    if (!mgr.hasTheme(themeName))
        return;
    mgr.setCurrentTheme(themeName);
}

void SettingsDialog::onCloseClicked() { accept(); }
