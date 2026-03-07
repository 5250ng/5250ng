#pragma once

#include "session_settings_dialog.h"
#include "ui/themes/manager.h"
#include "ui/themes/terminal_theme.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

class SettingsDialog : public ui::widgets::BaseFramelessDialog {
    Q_OBJECT

  public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    /// Access the embedded 5250 theme editor so callers can wire up signals.
    SessionSettingsDialog *terminalThemeEditor() const { return m_terminalThemePage; }

    /// Pre-select a terminal theme in the embedded editor.
    void setTerminalTheme(const ui::themes::TerminalTheme &theme);

  signals:
    void terminalThemeApplyRequested(const ui::themes::TerminalTheme &theme);
    void terminalThemeApplyToAllRequested(const ui::themes::TerminalTheme &theme);

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

    // Application Theme page widgets
    QWidget *m_themePage;
    QComboBox *m_themeCombo;
    QPushButton *m_applyThemeBtn;

    // 5250 Theme page (embedded SessionSettingsDialog)
    SessionSettingsDialog *m_terminalThemePage;
};
