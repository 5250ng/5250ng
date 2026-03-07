#pragma once

#include "theme_preview_widget.h"
#include "ui/themes/terminal_theme.h"
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QMap>

class SessionSettingsDialog : public QDialog {
    Q_OBJECT

  public:
    explicit SessionSettingsDialog(QWidget *parent = nullptr);

    void setTheme(const ui::themes::TerminalTheme &theme);
    ui::themes::TerminalTheme currentTheme() const;

    QString selectedThemeId() const;

  signals:
    void applyRequested(const ui::themes::TerminalTheme &theme);
    void applyToAllRequested(const ui::themes::TerminalTheme &theme);

  private slots:
    void onThemeDropdownChanged(int index);
    void onThemeFilterChanged(const QString &filter);
    void onNewTheme();
    void onDuplicateTheme();
    void onSaveTheme();
    void onDeleteTheme();
    void onResetTheme();
    void onRandomizeColors();
    void onImportTheme();
    void onExportTheme();
    void onApply();
    void onApplyToAll();
    void onOk();
    void onBackgroundModeChanged();
    void onBrowseBackgroundImage();
    void onColorSwatchClicked();
    void onThemePropertyChanged();
    void onMonochromeToggled(bool checked);

  private:
    void setupUI();
    QWidget *buildThemeSection();
    QWidget *buildBackgroundSection();
    QWidget *buildFontSection();
    QWidget *buildColorsSection();
    QWidget *buildSelectionSection();
    QWidget *buildColumnSeparatorSection();
    QWidget *buildCursorSection();
    QWidget *buildGridModeSection();
    QWidget *buildCRTSection();
    QWidget *buildBrightnessSection();

    void populateThemeDropdown();
    void loadThemeToUI(const ui::themes::TerminalTheme &theme);
    ui::themes::TerminalTheme collectThemeFromUI() const;
    void updatePreview();
    void updateContrastLabels();
    static double wcagContrastRatio(const QColor &fg, const QColor &bg);

    // Helper: create a color swatch button
    QPushButton *createColorSwatch(const QString &objectName, const QColor &initial);
    void setSwatchColor(QPushButton *btn, const QColor &color);
    QColor swatchColor(QPushButton *btn) const;

    // Theme dropdown
    QLineEdit *m_themeFilter;
    QComboBox *m_themeCombo;
    QPushButton *m_newBtn;
    QPushButton *m_duplicateBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_resetBtn;
    QPushButton *m_randomizeBtn;
    QPushButton *m_importBtn;
    QPushButton *m_exportBtn;

    // Theme info labels
    QLabel *m_parentThemeLabel;
    QLabel *m_descriptionLabel;

    // Background
    QRadioButton *m_bgColorRadio;
    QRadioButton *m_bgImageRadio;
    QPushButton *m_bgColorSwatch;
    QLineEdit *m_bgImagePath;
    QPushButton *m_bgBrowseBtn;
    QComboBox *m_bgLayoutCombo;
    QSlider *m_bgOpacitySlider;
    QLabel *m_bgOpacityLabel;
    QSlider *m_screenOpacitySlider;
    QLabel *m_screenOpacityLabel;

    // Font
    QComboBox *m_fontCombo;
    QSpinBox *m_fontSizeSpin;

    // Grid mode
    QComboBox *m_gridModeCombo;

    // Monochrome
    QCheckBox *m_monochromeEnabled;
    QPushButton *m_monochromeColorSwatch;

    // Terminal colors
    QPushButton *m_colorGreen;
    QPushButton *m_colorWhite;
    QPushButton *m_colorBlue;
    QPushButton *m_colorYellow;
    QPushButton *m_colorRed;
    QPushButton *m_colorCyan;
    QPushButton *m_colorPink;
    QPushButton *m_colorBlack;
    QPushButton *m_colorCursor;

    // Selection & indicators
    QPushButton *m_selBgSwatch;
    QPushButton *m_selFgSwatch;
    QPushButton *m_fieldIndicatorSwatch;

    // Column separator
    QPushButton *m_colSepColorSwatch;
    QComboBox *m_colSepStyleCombo;

    // Cursor
    QRadioButton *m_cursorBlock;
    QRadioButton *m_cursorUnderline;
    QRadioButton *m_cursorBar;
    QSpinBox *m_cursorBlinkSpin;

    // CRT effect
    QCheckBox *m_crtEnabled;
    QSlider *m_crtScanline;
    QSlider *m_crtPhosphorBloom;
    QSlider *m_crtGlow;
    QSlider *m_crtCurvature;

    // Brightness/saturation
    QSlider *m_brightnessSlider;
    QSlider *m_saturationSlider;
    QLabel *m_brightnessLabel;
    QLabel *m_saturationLabel;

    // Contrast ratio labels (color swatch -> label)
    QMap<QPushButton *, QLabel *> m_contrastLabels;

    // Preview
    ThemePreviewWidget *m_preview;

    // Current state
    QString m_currentThemeId;
};
