#pragma once

#include "ui/themes/manager.h"
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

class SettingsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

  private slots:
    void onCategoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onThemeChanged();
    void onApplyThemeClicked();
    void onCloseClicked();

  private:
    void setupUI();
    QWidget *buildThemePage();
    void ensureThemesLoaded();

    QSplitter *m_splitter;
    QTreeWidget *m_categoryTree;
    QStackedWidget *m_pages;

    // Theme page widgets
    QWidget *m_themePage;
    QComboBox *m_themeCombo;
    QPushButton *m_applyThemeBtn;
};
