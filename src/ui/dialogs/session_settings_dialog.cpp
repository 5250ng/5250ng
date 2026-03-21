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

#include "session_settings_dialog.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QRandomGenerator>
#include <QScrollArea>
#include <QVBoxLayout>
#include <cmath>

SessionSettingsDialog::SessionSettingsDialog(QWidget *parent) : QDialog(parent) {
    setupUI();
    populateThemeDropdown();
}

// --- Setup ---

void SessionSettingsDialog::setupUI() {
    setWindowTitle("Session Settings");
    resize(620, 780);

    QVBoxLayout *root = new QVBoxLayout(this);

    // Scrollable content
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *content = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setSpacing(12);

    contentLayout->addWidget(buildThemeSection());
    contentLayout->addWidget(buildGridModeSection());
    contentLayout->addWidget(buildBackgroundSection());
    contentLayout->addWidget(buildFontSection());
    contentLayout->addWidget(buildColorsSection());
    contentLayout->addWidget(buildSelectionSection());
    contentLayout->addWidget(buildColumnSeparatorSection());
    contentLayout->addWidget(buildHRuleSection());
    contentLayout->addWidget(buildFooterColorsSection());
    contentLayout->addWidget(buildCursorPositionSection());
    contentLayout->addWidget(buildCursorSection());
    contentLayout->addWidget(buildCRTSection());
    contentLayout->addWidget(buildBrightnessSection());

    // Preview
    QGroupBox *previewGroup = new QGroupBox("Preview", content);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    m_preview = new ThemePreviewWidget(previewGroup);
    previewLayout->addWidget(m_preview);
    contentLayout->addWidget(previewGroup);

    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // Bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *applyAllBtn = new QPushButton("Apply to All Sessions", this);
    QPushButton *applyBtn = new QPushButton("Apply", this);

    btnLayout->addWidget(applyAllBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(applyBtn);
    root->addLayout(btnLayout);

    connect(applyAllBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onApplyToAll);
    connect(applyBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onApply);
}

QWidget *SessionSettingsDialog::buildThemeSection() {
    QGroupBox *group = new QGroupBox("Theme");
    QVBoxLayout *v = new QVBoxLayout(group);

    // Filter row
    QHBoxLayout *filterRow = new QHBoxLayout();
    m_themeFilter = new QLineEdit();
    m_themeFilter->setPlaceholderText("Search themes...");
    m_themeFilter->setClearButtonEnabled(true);
    filterRow->addWidget(new QLabel("Filter:"));
    filterRow->addWidget(m_themeFilter, 1);
    v->addLayout(filterRow);

    // Dropdown row
    QHBoxLayout *dropdownRow = new QHBoxLayout();
    m_themeCombo = new QComboBox();
    dropdownRow->addWidget(new QLabel("Theme:"));
    dropdownRow->addWidget(m_themeCombo, 1);
    v->addLayout(dropdownRow);

    // Buttons row 1
    QHBoxLayout *btnRow = new QHBoxLayout();
    m_newBtn       = new QPushButton("New");
    m_duplicateBtn = new QPushButton("Duplicate");
    m_saveBtn      = new QPushButton("Save");
    m_deleteBtn    = new QPushButton("Delete");
    m_resetBtn     = new QPushButton("Reset");
    btnRow->addWidget(m_newBtn);
    btnRow->addWidget(m_duplicateBtn);
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_resetBtn);
    btnRow->addStretch();
    v->addLayout(btnRow);

    // Buttons row 2: Import/Export/Randomize
    QHBoxLayout *btnRow2 = new QHBoxLayout();
    m_importBtn    = new QPushButton("Import...");
    m_exportBtn    = new QPushButton("Export...");
    m_randomizeBtn = new QPushButton("Randomize");
    btnRow2->addWidget(m_importBtn);
    btnRow2->addWidget(m_exportBtn);
    btnRow2->addWidget(m_randomizeBtn);
    btnRow2->addStretch();
    v->addLayout(btnRow2);

    // Theme description label
    m_descriptionLabel = new QLabel();
    m_descriptionLabel->setStyleSheet("color: #999; padding: 2px 0;");
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setVisible(false);
    v->addWidget(m_descriptionLabel);

    // Parent theme info label
    m_parentThemeLabel = new QLabel();
    m_parentThemeLabel->setStyleSheet("color: #888; font-style: italic;");
    m_parentThemeLabel->setVisible(false);
    v->addWidget(m_parentThemeLabel);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SessionSettingsDialog::onThemeDropdownChanged);
    connect(m_newBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onNewTheme);
    connect(m_duplicateBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onDuplicateTheme);
    connect(m_saveBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onSaveTheme);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onDeleteTheme);
    connect(m_resetBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onResetTheme);
    connect(m_importBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onImportTheme);
    connect(m_exportBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onExportTheme);
    connect(m_randomizeBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onRandomizeColors);
    connect(m_themeFilter, &QLineEdit::textChanged, this, &SessionSettingsDialog::onThemeFilterChanged);

    return group;
}

QWidget *SessionSettingsDialog::buildGridModeSection() {
    QGroupBox *group = new QGroupBox("Screen Grid");
    QHBoxLayout *h = new QHBoxLayout(group);

    h->addWidget(new QLabel("Layout:"));
    m_gridModeCombo = new QComboBox();
    m_gridModeCombo->addItem("Packed", "packed");
    m_gridModeCombo->addItem("Wide", "wide");
    m_gridModeCombo->setToolTip(
        "Packed: characters are tightly spaced.\n"
        "Wide: characters have extra horizontal spacing between columns.");
    h->addWidget(m_gridModeCombo);

    h->addSpacing(20);
    h->addWidget(new QLabel("Grid Color:"));
    m_cellGridColorSwatch = createColorSwatch("cellGrid", QColor(255, 255, 255, 40));
    h->addWidget(m_cellGridColorSwatch);
    connect(m_cellGridColorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    h->addStretch();

    connect(m_gridModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SessionSettingsDialog::onThemePropertyChanged);

    return group;
}

QWidget *SessionSettingsDialog::buildBackgroundSection() {
    QGroupBox *group = new QGroupBox("Background");
    QVBoxLayout *v = new QVBoxLayout(group);

    // Mode selection
    QHBoxLayout *modeRow = new QHBoxLayout();
    m_bgColorRadio = new QRadioButton("Color");
    m_bgImageRadio = new QRadioButton("Image");
    m_bgColorRadio->setChecked(true);
    modeRow->addWidget(new QLabel("Mode:"));
    modeRow->addWidget(m_bgColorRadio);
    modeRow->addWidget(m_bgImageRadio);
    modeRow->addStretch();
    v->addLayout(modeRow);

    // Color row
    QHBoxLayout *colorRow = new QHBoxLayout();
    colorRow->addWidget(new QLabel("Background Color:"));
    m_bgColorSwatch = createColorSwatch("bgColor", QColor(0, 0, 0));
    colorRow->addWidget(m_bgColorSwatch);
    colorRow->addStretch();
    v->addLayout(colorRow);

    // Image row
    QHBoxLayout *imageRow = new QHBoxLayout();
    imageRow->addWidget(new QLabel("Background Image:"));
    m_bgImagePath = new QLineEdit();
    m_bgImagePath->setPlaceholderText("Path to image...");
    m_bgBrowseBtn = new QPushButton("Browse...");
    imageRow->addWidget(m_bgImagePath, 1);
    imageRow->addWidget(m_bgBrowseBtn);
    v->addLayout(imageRow);

    // Layout & opacity
    QHBoxLayout *layoutRow = new QHBoxLayout();
    layoutRow->addWidget(new QLabel("Layout:"));
    m_bgLayoutCombo = new QComboBox();
    m_bgLayoutCombo->addItem("Stretch", "stretch");
    m_bgLayoutCombo->addItem("Tile", "tile");
    m_bgLayoutCombo->addItem("Center", "center");
    m_bgLayoutCombo->addItem("Fit", "fit");
    layoutRow->addWidget(m_bgLayoutCombo);
    layoutRow->addSpacing(20);
    layoutRow->addWidget(new QLabel("Opacity:"));
    m_bgOpacitySlider = new QSlider(Qt::Horizontal);
    m_bgOpacitySlider->setRange(0, 100);
    m_bgOpacitySlider->setValue(100);
    m_bgOpacityLabel = new QLabel("1.00");
    layoutRow->addWidget(m_bgOpacitySlider, 1);
    layoutRow->addWidget(m_bgOpacityLabel);
    v->addLayout(layoutRow);

    // Screen opacity - controls how much the background color covers the image
    QHBoxLayout *screenOpacityRow = new QHBoxLayout();
    screenOpacityRow->addWidget(new QLabel("Screen Opacity:"));
    m_screenOpacitySlider = new QSlider(Qt::Horizontal);
    m_screenOpacitySlider->setRange(0, 100);
    m_screenOpacitySlider->setValue(100);
    m_screenOpacitySlider->setToolTip(
        "Controls how opaque the terminal background is.\n"
        "Lower values let the background image show through.");
    m_screenOpacityLabel = new QLabel("1.00");
    screenOpacityRow->addWidget(m_screenOpacitySlider, 1);
    screenOpacityRow->addWidget(m_screenOpacityLabel);
    v->addLayout(screenOpacityRow);

    connect(m_bgColorRadio, &QRadioButton::toggled, this, &SessionSettingsDialog::onBackgroundModeChanged);
    connect(m_bgBrowseBtn, &QPushButton::clicked, this, &SessionSettingsDialog::onBrowseBackgroundImage);
    connect(m_bgOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_bgOpacityLabel->setText(QString::number(val / 100.0, 'f', 2));
        onThemePropertyChanged();
    });
    connect(m_screenOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_screenOpacityLabel->setText(QString::number(val / 100.0, 'f', 2));
        onThemePropertyChanged();
    });
    connect(m_bgColorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    return group;
}

QWidget *SessionSettingsDialog::buildFontSection() {
    QGroupBox *group = new QGroupBox("Font");
    QHBoxLayout *h = new QHBoxLayout(group);

    h->addWidget(new QLabel("Font:"));
    m_fontCombo = new QComboBox();
    // Populate with monospace fonts
    QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        if (QFontDatabase::isFixedPitch(family)) {
            m_fontCombo->addItem(family);
        }
    }
    h->addWidget(m_fontCombo, 1);

    h->addSpacing(20);
    h->addWidget(new QLabel("Size:"));
    m_fontSizeSpin = new QSpinBox();
    m_fontSizeSpin->setRange(6, 48);
    m_fontSizeSpin->setValue(14);
    h->addWidget(m_fontSizeSpin);

    connect(m_fontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SessionSettingsDialog::onThemePropertyChanged);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SessionSettingsDialog::onThemePropertyChanged);

    return group;
}

QWidget *SessionSettingsDialog::buildColorsSection() {
    QGroupBox *group = new QGroupBox("Terminal Colors");
    QVBoxLayout *outer = new QVBoxLayout(group);

    // Monochrome option
    QHBoxLayout *monoRow = new QHBoxLayout();
    m_monochromeEnabled = new QCheckBox("Monochrome");
    m_monochromeEnabled->setToolTip("Derive all colors from a single hue");
    monoRow->addWidget(m_monochromeEnabled);
    monoRow->addWidget(new QLabel("Color:"));
    m_monochromeColorSwatch = createColorSwatch("monoColor", QColor(255, 176, 0));
    monoRow->addWidget(m_monochromeColorSwatch);
    monoRow->addStretch();
    outer->addLayout(monoRow);

    connect(m_monochromeEnabled, &QCheckBox::toggled, this, &SessionSettingsDialog::onMonochromeToggled);
    connect(m_monochromeColorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    // Individual color grid
    QGridLayout *grid = new QGridLayout();

    // Each column group: label(0), swatch(1), contrast(2), spacer(3), label(4), swatch(5), contrast(6)
    auto addColorRow = [&](int row, int col, const QString &label,
                           QPushButton *&swatch, const QColor &initial) {
        int base = col * 4;
        grid->addWidget(new QLabel(label), row, base);
        swatch = createColorSwatch(label.toLower().trimmed(), initial);
        grid->addWidget(swatch, row, base + 1);
        QLabel *crLabel = new QLabel();
        crLabel->setStyleSheet("color: #aaa; font-size: 10px;");
        crLabel->setMinimumWidth(50);
        grid->addWidget(crLabel, row, base + 2);
        m_contrastLabels[swatch] = crLabel;
        connect(swatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);
    };

    // Left column               Right column
    addColorRow(0, 0, "Green:",   m_colorGreen,  QColor(0, 255, 0));
    addColorRow(0, 1, "White:",   m_colorWhite,  QColor(255, 255, 255));
    addColorRow(1, 0, "Blue:",    m_colorBlue,   QColor(0, 0, 255));
    addColorRow(1, 1, "Yellow:",  m_colorYellow, QColor(255, 255, 0));
    addColorRow(2, 0, "Red:",     m_colorRed,    QColor(255, 0, 0));
    addColorRow(2, 1, "Cyan:",    m_colorCyan,   QColor(0, 255, 255));
    addColorRow(3, 0, "Pink:",    m_colorPink,   QColor(255, 0, 255));
    addColorRow(3, 1, "Black:",   m_colorBlack,  QColor(0, 0, 0));
    addColorRow(4, 0, "Cursor:",  m_colorCursor, QColor(0, 255, 0));

    // Spacer column between left and right groups
    grid->setColumnMinimumWidth(3, 20);

    outer->addLayout(grid);

    return group;
}

QWidget *SessionSettingsDialog::buildSelectionSection() {
    QGroupBox *group = new QGroupBox("Selection & Indicators");
    QGridLayout *grid = new QGridLayout(group);

    grid->addWidget(new QLabel("Selection BG:"), 0, 0);
    m_selBgSwatch = createColorSwatch("selBg", QColor(255, 255, 0, 64));
    grid->addWidget(m_selBgSwatch, 0, 1);
    connect(m_selBgSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->addWidget(new QLabel("Selection FG:"), 0, 3);
    m_selFgSwatch = createColorSwatch("selFg", QColor(255, 255, 255));
    grid->addWidget(m_selFgSwatch, 0, 4);
    connect(m_selFgSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->addWidget(new QLabel("Field Indicator:"), 1, 0);
    m_fieldIndicatorSwatch = createColorSwatch("fieldInd", QColor(0, 128, 255, 64));
    grid->addWidget(m_fieldIndicatorSwatch, 1, 1);
    connect(m_fieldIndicatorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->setColumnMinimumWidth(2, 20);

    return group;
}

QWidget *SessionSettingsDialog::buildColumnSeparatorSection() {
    QGroupBox *group = new QGroupBox("Column Separator");
    QHBoxLayout *h = new QHBoxLayout(group);

    m_colSepEnabled = new QCheckBox("Enabled");
    h->addWidget(m_colSepEnabled);
    connect(m_colSepEnabled, &QCheckBox::toggled, this, &SessionSettingsDialog::onThemePropertyChanged);

    h->addSpacing(20);
    h->addWidget(new QLabel("Color:"));
    m_colSepColorSwatch = createColorSwatch("colSep", QColor(128, 128, 128));
    h->addWidget(m_colSepColorSwatch);
    connect(m_colSepColorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    h->addSpacing(20);
    h->addWidget(new QLabel("Style:"));
    m_colSepStyleCombo = new QComboBox();
    m_colSepStyleCombo->addItem("Solid", "solid");
    m_colSepStyleCombo->addItem("Dotted", "dotted");
    m_colSepStyleCombo->addItem("Dimmed", "dimmed");
    h->addWidget(m_colSepStyleCombo);
    h->addStretch();

    connect(m_colSepStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SessionSettingsDialog::onThemePropertyChanged);

    return group;
}

QWidget *SessionSettingsDialog::buildHRuleSection() {
    QGroupBox *group = new QGroupBox("Horizontal Rule");
    QHBoxLayout *h = new QHBoxLayout(group);

    h->addWidget(new QLabel("Color:"));
    m_hruleColorSwatch = createColorSwatch("hruleColor", QColor(0, 255, 0));
    h->addWidget(m_hruleColorSwatch);
    connect(m_hruleColorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);
    h->addStretch();

    return group;
}

QWidget *SessionSettingsDialog::buildFooterColorsSection() {
    QGroupBox *group = new QGroupBox("Footer Colors");
    QGridLayout *grid = new QGridLayout(group);

    grid->addWidget(new QLabel("Keyboard State:"), 0, 0);
    m_footerKbdStateSwatch = createColorSwatch("footerKbd", QColor());
    grid->addWidget(m_footerKbdStateSwatch, 0, 1);
    connect(m_footerKbdStateSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->addWidget(new QLabel("System Name:"), 0, 3);
    m_footerSystemNameSwatch = createColorSwatch("footerSys", QColor());
    grid->addWidget(m_footerSystemNameSwatch, 0, 4);
    connect(m_footerSystemNameSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->addWidget(new QLabel("History:"), 1, 0);
    m_footerHistorySwatch = createColorSwatch("footerHist", QColor());
    grid->addWidget(m_footerHistorySwatch, 1, 1);
    connect(m_footerHistorySwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->addWidget(new QLabel("Macro:"), 1, 3);
    m_footerMacroSwatch = createColorSwatch("footerMacro", QColor());
    grid->addWidget(m_footerMacroSwatch, 1, 4);
    connect(m_footerMacroSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    grid->setColumnMinimumWidth(2, 20);

    return group;
}

QWidget *SessionSettingsDialog::buildCursorPositionSection() {
    QGroupBox *group = new QGroupBox("Cursor Position");
    QHBoxLayout *h = new QHBoxLayout(group);

    m_cursorPosCustom = new QCheckBox("Custom color");
    m_cursorPosCustom->setToolTip(
        "When unchecked, the cursor position uses the theme's \"white\" color.");
    h->addWidget(m_cursorPosCustom);

    h->addSpacing(10);
    h->addWidget(new QLabel("Color:"));
    m_cursorPosColorSwatch = createColorSwatch("cursorPosColor", QColor(255, 255, 255));
    m_cursorPosColorSwatch->setEnabled(false);
    h->addWidget(m_cursorPosColorSwatch);
    h->addStretch();

    connect(m_cursorPosCustom, &QCheckBox::toggled, this, [this](bool checked) {
        m_cursorPosColorSwatch->setEnabled(checked);
        onThemePropertyChanged();
    });
    connect(m_cursorPosColorSwatch, &QPushButton::clicked, this, &SessionSettingsDialog::onColorSwatchClicked);

    return group;
}

QWidget *SessionSettingsDialog::buildCursorSection() {
    QGroupBox *group = new QGroupBox("Cursor");
    QVBoxLayout *v = new QVBoxLayout(group);

    QHBoxLayout *shapeRow = new QHBoxLayout();
    shapeRow->addWidget(new QLabel("Shape:"));
    m_cursorBlock = new QRadioButton("Block");
    m_cursorUnderline = new QRadioButton("Underline");
    m_cursorBar = new QRadioButton("Bar");
    m_cursorBlock->setChecked(true);
    shapeRow->addWidget(m_cursorBlock);
    shapeRow->addWidget(m_cursorUnderline);
    shapeRow->addWidget(m_cursorBar);
    shapeRow->addStretch();
    v->addLayout(shapeRow);

    QHBoxLayout *blinkRow = new QHBoxLayout();
    blinkRow->addWidget(new QLabel("Blink rate (ms):"));
    m_cursorBlinkSpin = new QSpinBox();
    m_cursorBlinkSpin->setRange(0, 2000);
    m_cursorBlinkSpin->setSingleStep(50);
    m_cursorBlinkSpin->setValue(530);
    m_cursorBlinkSpin->setSpecialValueText("No blink");
    blinkRow->addWidget(m_cursorBlinkSpin);
    blinkRow->addStretch();
    v->addLayout(blinkRow);

    connect(m_cursorBlock, &QRadioButton::toggled, this, &SessionSettingsDialog::onThemePropertyChanged);
    connect(m_cursorUnderline, &QRadioButton::toggled, this, &SessionSettingsDialog::onThemePropertyChanged);
    connect(m_cursorBar, &QRadioButton::toggled, this, &SessionSettingsDialog::onThemePropertyChanged);
    connect(m_cursorBlinkSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SessionSettingsDialog::onThemePropertyChanged);

    return group;
}

QWidget *SessionSettingsDialog::buildCRTSection() {
    QGroupBox *group = new QGroupBox("CRT Effect");
    QVBoxLayout *v = new QVBoxLayout(group);

    m_crtEnabled = new QCheckBox("Enable CRT effect");
    v->addWidget(m_crtEnabled);

    auto addSliderRow = [&](const QString &label, QSlider *&slider, QLabel *&valLabel,
                            const QString &tooltip) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(label);
        if (!tooltip.isEmpty()) nameLabel->setToolTip(tooltip);
        row->addWidget(nameLabel);
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(30);
        if (!tooltip.isEmpty()) slider->setToolTip(tooltip);
        valLabel = new QLabel(QString::number(slider->value()) + "%");
        row->addWidget(slider, 1);
        row->addWidget(valLabel);
        v->addLayout(row);
        connect(slider, &QSlider::valueChanged, this, [this, valLabel](int val) {
            valLabel->setText(QString::number(val) + "%");
            onThemePropertyChanged();
        });
    };

    addSliderRow("Scanlines:", m_crtScanline, m_crtScanlineLabel,
                 "Intensity of horizontal scanline overlay");
    addSliderRow("Phosphor Bloom:", m_crtPhosphorBloom, m_crtPhosphorBloomLabel,
                 "Local glow around bright characters");
    addSliderRow("Glow:", m_crtGlow, m_crtGlowLabel,
                 "Radial phosphor glow emanating from the screen center");
    addSliderRow("Curvature:", m_crtCurvature, m_crtCurvatureLabel,
                 "Vignette darkening at screen edges simulating CRT curvature");

    connect(m_crtEnabled, &QCheckBox::toggled, this, &SessionSettingsDialog::onThemePropertyChanged);

    return group;
}

QWidget *SessionSettingsDialog::buildBrightnessSection() {
    QGroupBox *group = new QGroupBox("Brightness & Saturation");
    QVBoxLayout *v = new QVBoxLayout(group);

    auto addSlider = [&](const QString &label, QSlider *&slider, QLabel *&valLabel,
                         int minVal, int maxVal, int initial) {
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new QLabel(label));
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(minVal, maxVal);
        slider->setValue(initial);
        valLabel = new QLabel(QString::number(initial) + "%");
        row->addWidget(slider, 1);
        row->addWidget(valLabel);
        v->addLayout(row);
    };

    addSlider("Brightness:", m_brightnessSlider, m_brightnessLabel, 0, 200, 100);
    addSlider("Saturation:", m_saturationSlider, m_saturationLabel, 0, 200, 100);

    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int val) {
        m_brightnessLabel->setText(QString::number(val) + "%");
        onThemePropertyChanged();
    });
    connect(m_saturationSlider, &QSlider::valueChanged, this, [this](int val) {
        m_saturationLabel->setText(QString::number(val) + "%");
        onThemePropertyChanged();
    });

    return group;
}

// --- Color swatch helpers ---

QPushButton *SessionSettingsDialog::createColorSwatch(const QString &objectName,
                                                       const QColor &initial) {
    QPushButton *btn = new QPushButton();
    btn->setObjectName(objectName);
    btn->setFixedSize(40, 24);
    btn->setCursor(Qt::PointingHandCursor);
    setSwatchColor(btn, initial);
    return btn;
}

void SessionSettingsDialog::setSwatchColor(QPushButton *btn, const QColor &color) {
    btn->setProperty("swatchColor", color);
    btn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #888; border-radius: 3px;")
            .arg(color.name(QColor::HexArgb)));
}

QColor SessionSettingsDialog::swatchColor(QPushButton *btn) const {
    return btn->property("swatchColor").value<QColor>();
}

void SessionSettingsDialog::onColorSwatchClicked() {
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) return;

    QColor current = swatchColor(btn);
    QColorDialog dlg(current, this);
    dlg.setOption(QColorDialog::ShowAlphaChannel, true);
    if (dlg.exec() == QDialog::Accepted) {
        setSwatchColor(btn, dlg.selectedColor());
        onThemePropertyChanged();
    }
}

// --- Theme dropdown ---

void SessionSettingsDialog::populateThemeDropdown() {
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    QString filter = m_themeFilter ? m_themeFilter->text().trimmed() : QString();
    m_themeCombo->blockSignals(true);
    m_themeCombo->clear();
    for (const QString &id : mgr.availableThemes()) {
        ui::themes::TerminalTheme t = mgr.theme(id);
        // Apply filter
        if (!filter.isEmpty()) {
            if (!t.displayName.contains(filter, Qt::CaseInsensitive)
                && !t.description.contains(filter, Qt::CaseInsensitive)
                && !id.contains(filter, Qt::CaseInsensitive)) {
                continue;
            }
        }
        QString label = t.displayName;
        if (t.builtin) label += " (built-in)";
        m_themeCombo->addItem(label, id);
    }
    m_themeCombo->blockSignals(false);

    // Select default
    int idx = m_themeCombo->findData(m_currentThemeId);
    if (idx < 0) idx = m_themeCombo->findData("classic_green");
    if (idx < 0 && m_themeCombo->count() > 0) idx = 0;
    if (idx >= 0) {
        m_themeCombo->setCurrentIndex(idx);
        onThemeDropdownChanged(idx);
    }
}

void SessionSettingsDialog::onThemeDropdownChanged(int index) {
    if (index < 0) return;
    QString id = m_themeCombo->itemData(index).toString();
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    ui::themes::TerminalTheme rawTheme = mgr.theme(id);
    ui::themes::TerminalTheme theme = mgr.resolvedTheme(id);
    if (theme.id.isEmpty()) return;
    m_currentThemeId = id;
    loadThemeToUI(theme);
    updatePreview();

    // Disable delete/save for builtin themes
    m_deleteBtn->setEnabled(!rawTheme.builtin);
    m_saveBtn->setEnabled(!rawTheme.builtin);
    m_resetBtn->setEnabled(true);

    // Show theme description if present
    if (!rawTheme.description.isEmpty()) {
        m_descriptionLabel->setText(rawTheme.description);
        m_descriptionLabel->setVisible(true);
    } else {
        m_descriptionLabel->setVisible(false);
    }

    // Show parent theme info if applicable
    if (!rawTheme.parentThemeId.isEmpty() && mgr.hasTheme(rawTheme.parentThemeId)) {
        auto parent = mgr.theme(rawTheme.parentThemeId);
        m_parentThemeLabel->setText(
            QString("Inherits from: %1").arg(parent.displayName));
        m_parentThemeLabel->setVisible(true);
    } else {
        m_parentThemeLabel->setVisible(false);
    }
}

// --- Load/collect theme ---

void SessionSettingsDialog::setTheme(const ui::themes::TerminalTheme &theme) {
    m_currentThemeId = theme.id;
    int idx = m_themeCombo->findData(theme.id);
    if (idx >= 0) {
        m_themeCombo->setCurrentIndex(idx);
    }
    loadThemeToUI(theme);
    updatePreview();
}

void SessionSettingsDialog::loadThemeToUI(const ui::themes::TerminalTheme &theme) {
    // Grid mode & cell grid color
    int gmIdx = m_gridModeCombo->findData(
        ui::themes::TerminalTheme::gridModeToString(theme.gridMode));
    if (gmIdx >= 0) m_gridModeCombo->setCurrentIndex(gmIdx);
    setSwatchColor(m_cellGridColorSwatch, theme.cellGridColor);

    // Background
    m_bgColorRadio->setChecked(theme.backgroundMode == ui::themes::TerminalTheme::Color);
    m_bgImageRadio->setChecked(theme.backgroundMode == ui::themes::TerminalTheme::Image);
    setSwatchColor(m_bgColorSwatch, theme.backgroundColor);
    m_bgImagePath->setText(theme.backgroundImagePath);
    int layoutIdx = m_bgLayoutCombo->findData(
        ui::themes::TerminalTheme::imageLayoutToString(theme.backgroundImageLayout));
    if (layoutIdx >= 0) m_bgLayoutCombo->setCurrentIndex(layoutIdx);
    m_bgOpacitySlider->setValue(static_cast<int>(theme.backgroundImageOpacity * 100));
    m_screenOpacitySlider->setValue(static_cast<int>(theme.screenBackgroundOpacity * 100));

    // Font
    int fontIdx = m_fontCombo->findText(theme.fontFamily);
    if (fontIdx >= 0) m_fontCombo->setCurrentIndex(fontIdx);
    m_fontSizeSpin->setValue(theme.fontSize);

    // Monochrome
    m_monochromeEnabled->setChecked(theme.monochrome);
    setSwatchColor(m_monochromeColorSwatch, theme.monochromeColor);
    onMonochromeToggled(theme.monochrome);

    // Colors
    setSwatchColor(m_colorGreen,  theme.colorGreen);
    setSwatchColor(m_colorWhite,  theme.colorWhite);
    setSwatchColor(m_colorBlue,   theme.colorBlue);
    setSwatchColor(m_colorYellow, theme.colorYellow);
    setSwatchColor(m_colorRed,    theme.colorRed);
    setSwatchColor(m_colorCyan,   theme.colorCyan);
    setSwatchColor(m_colorPink,   theme.colorPink);
    setSwatchColor(m_colorBlack,  theme.colorBlack);
    setSwatchColor(m_colorCursor, theme.cursorColor);

    // Selection
    setSwatchColor(m_selBgSwatch, theme.selectionBackground);
    setSwatchColor(m_selFgSwatch, theme.selectionForeground);
    setSwatchColor(m_fieldIndicatorSwatch, theme.fieldIndicatorColor);

    // Column separator
    m_colSepEnabled->setChecked(theme.columnSeparatorEnabled);
    setSwatchColor(m_colSepColorSwatch, theme.columnSeparatorColor);
    int csIdx = m_colSepStyleCombo->findData(
        ui::themes::TerminalTheme::colSepStyleToString(theme.columnSeparatorStyle));
    if (csIdx >= 0) m_colSepStyleCombo->setCurrentIndex(csIdx);

    // HRule
    setSwatchColor(m_hruleColorSwatch, theme.hruleColor.isValid() ? theme.hruleColor : theme.colorGreen);

    // Footer colors
    setSwatchColor(m_footerKbdStateSwatch,
                   theme.footerKbdStateColor.isValid() ? theme.footerKbdStateColor : theme.colorWhite);
    setSwatchColor(m_footerSystemNameSwatch,
                   theme.footerSystemNameColor.isValid() ? theme.footerSystemNameColor : theme.colorWhite);
    setSwatchColor(m_footerHistorySwatch,
                   theme.footerHistoryColor.isValid() ? theme.footerHistoryColor : theme.colorWhite);
    setSwatchColor(m_footerMacroSwatch,
                   theme.footerMacroColor.isValid() ? theme.footerMacroColor : theme.colorWhite);

    // Cursor position
    bool hasCustomCoord = theme.footerCoordinatesColor.isValid();
    m_cursorPosCustom->setChecked(hasCustomCoord);
    m_cursorPosColorSwatch->setEnabled(hasCustomCoord);
    setSwatchColor(m_cursorPosColorSwatch,
                   hasCustomCoord ? theme.footerCoordinatesColor : theme.colorWhite);

    // Cursor
    m_cursorBlock->setChecked(theme.cursorShape == ui::themes::TerminalTheme::Block);
    m_cursorUnderline->setChecked(theme.cursorShape == ui::themes::TerminalTheme::Underline);
    m_cursorBar->setChecked(theme.cursorShape == ui::themes::TerminalTheme::Bar);
    m_cursorBlinkSpin->setValue(theme.cursorBlinkRateMs);

    // CRT
    m_crtEnabled->setChecked(theme.crtEffectEnabled);
    m_crtScanline->setValue(static_cast<int>(theme.crtScanlineIntensity * 100));
    m_crtPhosphorBloom->setValue(static_cast<int>(theme.crtPhosphorBloom * 100));
    m_crtGlow->setValue(static_cast<int>(theme.crtGlowRadius * 100));
    m_crtCurvature->setValue(static_cast<int>(theme.crtCurvature * 100));

    // Brightness/saturation
    m_brightnessSlider->setValue(qRound(theme.globalBrightness * 100));
    m_saturationSlider->setValue(qRound(theme.globalSaturation * 100));
    m_brightnessLabel->setText(QString::number(qRound(theme.globalBrightness * 100)) + "%");
    m_saturationLabel->setText(QString::number(qRound(theme.globalSaturation * 100)) + "%");

    onBackgroundModeChanged();
    updateContrastLabels();
}

ui::themes::TerminalTheme SessionSettingsDialog::collectThemeFromUI() const {
    ui::themes::TerminalTheme t;
    t.id = m_currentThemeId;
    t.displayName = m_themeCombo->currentText();
    // Remove " (built-in)" suffix if present
    if (t.displayName.endsWith(" (built-in)")) {
        t.displayName.chop(11);
    }

    // Grid mode & cell grid color
    t.gridMode = ui::themes::TerminalTheme::gridModeFromString(
        m_gridModeCombo->currentData().toString());
    t.cellGridColor = swatchColor(m_cellGridColorSwatch);

    // Background
    t.backgroundMode = m_bgColorRadio->isChecked()
                            ? ui::themes::TerminalTheme::Color
                            : ui::themes::TerminalTheme::Image;
    t.backgroundColor = swatchColor(m_bgColorSwatch);
    t.backgroundImagePath = m_bgImagePath->text();
    t.backgroundImageLayout = ui::themes::TerminalTheme::imageLayoutFromString(
        m_bgLayoutCombo->currentData().toString());
    t.backgroundImageOpacity = m_bgOpacitySlider->value() / 100.0;
    t.screenBackgroundOpacity = m_screenOpacitySlider->value() / 100.0;

    // Font
    t.fontFamily = m_fontCombo->currentText();
    t.fontSize   = m_fontSizeSpin->value();

    // Monochrome
    t.monochrome = m_monochromeEnabled->isChecked();
    t.monochromeColor = swatchColor(m_monochromeColorSwatch);

    // Colors
    t.colorGreen  = swatchColor(m_colorGreen);
    t.colorWhite  = swatchColor(m_colorWhite);
    t.colorBlue   = swatchColor(m_colorBlue);
    t.colorYellow = swatchColor(m_colorYellow);
    t.colorRed    = swatchColor(m_colorRed);
    t.colorCyan   = swatchColor(m_colorCyan);
    t.colorPink   = swatchColor(m_colorPink);
    t.colorBlack  = swatchColor(m_colorBlack);
    t.cursorColor = swatchColor(m_colorCursor);

    // Selection
    t.selectionBackground = swatchColor(m_selBgSwatch);
    t.selectionForeground = swatchColor(m_selFgSwatch);
    t.fieldIndicatorColor = swatchColor(m_fieldIndicatorSwatch);

    // Column separator
    t.columnSeparatorEnabled = m_colSepEnabled->isChecked();
    t.columnSeparatorColor = swatchColor(m_colSepColorSwatch);
    t.columnSeparatorStyle = ui::themes::TerminalTheme::colSepStyleFromString(
        m_colSepStyleCombo->currentData().toString());

    // HRule
    t.hruleColor = swatchColor(m_hruleColorSwatch);

    // Footer colors
    t.footerKbdStateColor = swatchColor(m_footerKbdStateSwatch);
    t.footerSystemNameColor = swatchColor(m_footerSystemNameSwatch);
    t.footerHistoryColor = swatchColor(m_footerHistorySwatch);
    t.footerMacroColor = swatchColor(m_footerMacroSwatch);

    // Cursor position
    if (m_cursorPosCustom->isChecked()) {
        t.footerCoordinatesColor = swatchColor(m_cursorPosColorSwatch);
    } else {
        t.footerCoordinatesColor = QColor(); // invalid = derive from white at runtime
    }

    // Cursor
    if (m_cursorUnderline->isChecked())      t.cursorShape = ui::themes::TerminalTheme::Underline;
    else if (m_cursorBar->isChecked())        t.cursorShape = ui::themes::TerminalTheme::Bar;
    else                                      t.cursorShape = ui::themes::TerminalTheme::Block;
    t.cursorBlinkRateMs = m_cursorBlinkSpin->value();

    // CRT
    t.crtEffectEnabled     = m_crtEnabled->isChecked();
    t.crtScanlineIntensity = m_crtScanline->value() / 100.0;
    t.crtPhosphorBloom     = m_crtPhosphorBloom->value() / 100.0;
    t.crtGlowRadius        = m_crtGlow->value() / 100.0;
    t.crtCurvature         = m_crtCurvature->value() / 100.0;

    // Brightness/saturation
    t.globalBrightness = m_brightnessSlider->value() / 100.0;
    t.globalSaturation = m_saturationSlider->value() / 100.0;

    return t;
}

ui::themes::TerminalTheme SessionSettingsDialog::currentTheme() const {
    return collectThemeFromUI();
}

QString SessionSettingsDialog::selectedThemeId() const {
    return m_currentThemeId;
}

// --- Actions ---

void SessionSettingsDialog::onBackgroundModeChanged() {
    bool isColor = m_bgColorRadio->isChecked();
    m_bgColorSwatch->setEnabled(isColor);
    m_bgImagePath->setEnabled(!isColor);
    m_bgBrowseBtn->setEnabled(!isColor);
    m_bgLayoutCombo->setEnabled(!isColor);
    m_bgOpacitySlider->setEnabled(!isColor);
    m_screenOpacitySlider->setEnabled(!isColor);
    onThemePropertyChanged();
}

void SessionSettingsDialog::onBrowseBackgroundImage() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select Background Image", QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!path.isEmpty()) {
        m_bgImagePath->setText(path);
        onThemePropertyChanged();
    }
}

void SessionSettingsDialog::onThemePropertyChanged() {
    updatePreview();
    updateContrastLabels();
}

void SessionSettingsDialog::updatePreview() {
    m_preview->setTheme(collectThemeFromUI());
}

void SessionSettingsDialog::onNewTheme() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Theme", "Theme name:", QLineEdit::Normal,
                                          "My Custom Theme", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    auto &mgr = ui::themes::TerminalThemeManager::instance();
    ui::themes::TerminalTheme t = collectThemeFromUI();
    // Generate a new unique id via duplicateTheme, then apply all UI properties
    ui::themes::TerminalTheme dup = mgr.duplicateTheme(m_currentThemeId, name.trimmed());
    t.id = dup.id;
    t.displayName = name.trimmed();
    t.builtin = false;

    mgr.saveUserTheme(t);
    m_currentThemeId = t.id;
    populateThemeDropdown();
}

void SessionSettingsDialog::onDuplicateTheme() {
    bool ok;
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    ui::themes::TerminalTheme original = mgr.theme(m_currentThemeId);
    QString baseName = original.displayName + " (copy)";
    QString name = QInputDialog::getText(this, "Duplicate Theme", "Theme name:", QLineEdit::Normal,
                                          baseName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    ui::themes::TerminalTheme dup = mgr.duplicateTheme(m_currentThemeId, name.trimmed());
    if (dup.id.isEmpty()) return;

    mgr.saveUserTheme(dup);
    m_currentThemeId = dup.id;
    populateThemeDropdown();
}

void SessionSettingsDialog::onSaveTheme() {
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    ui::themes::TerminalTheme existing = mgr.theme(m_currentThemeId);

    if (existing.builtin) {
        ui::widgets::StyledMessageBox::warning(this, "Save Theme",
                             "Cannot overwrite a built-in theme. Use 'Duplicate' first.");
        return;
    }

    ui::themes::TerminalTheme t = collectThemeFromUI();
    t.id = m_currentThemeId;
    t.builtin = false;
    mgr.saveUserTheme(t);
    ui::widgets::StyledMessageBox::information(this, "Save Theme", "Theme saved successfully.");
}

void SessionSettingsDialog::onResetTheme() {
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    if (!mgr.hasTheme(m_currentThemeId)) return;

    auto result = ui::widgets::StyledMessageBox::question(this, "Reset Theme",
        QString("Reset all settings to the stored values of \"%1\"?")
            .arg(m_themeCombo->currentText().remove(" (built-in)")));
    if (result != ui::widgets::StyledMessageBox::Yes) return;

    ui::themes::TerminalTheme theme = mgr.resolvedTheme(m_currentThemeId);
    loadThemeToUI(theme);
    updatePreview();
}

void SessionSettingsDialog::onDeleteTheme() {
    auto &mgr = ui::themes::TerminalThemeManager::instance();
    ui::themes::TerminalTheme t = mgr.theme(m_currentThemeId);
    if (t.builtin) {
        ui::widgets::StyledMessageBox::warning(this, "Delete Theme", "Cannot delete a built-in theme.");
        return;
    }

    auto result = ui::widgets::StyledMessageBox::question(this, "Delete Theme",
                                         QString("Delete theme \"%1\"?").arg(t.displayName));
    if (result != ui::widgets::StyledMessageBox::Yes) return;

    mgr.deleteUserTheme(m_currentThemeId);
    m_currentThemeId = "classic_green";
    populateThemeDropdown();
}

void SessionSettingsDialog::onImportTheme() {
    QString path = QFileDialog::getOpenFileName(
        this, "Import Theme", QString(),
        "Theme files (*.json *.5250theme)");
    if (path.isEmpty()) return;

    auto &mgr = ui::themes::TerminalThemeManager::instance();
    if (mgr.importTheme(path)) {
        populateThemeDropdown();
        ui::widgets::StyledMessageBox::information(this, "Import", "Theme imported successfully.");
    } else {
        ui::widgets::StyledMessageBox::warning(this, "Import", "Failed to import theme.");
    }
}

void SessionSettingsDialog::onExportTheme() {
    QString path = QFileDialog::getSaveFileName(
        this, "Export Theme", m_currentThemeId + ".5250theme",
        "Theme files (*.5250theme *.json)");
    if (path.isEmpty()) return;

    auto &mgr = ui::themes::TerminalThemeManager::instance();
    // First save current UI state temporarily
    ui::themes::TerminalTheme t = collectThemeFromUI();
    t.id = m_currentThemeId;
    mgr.registerTheme(t);

    if (mgr.exportTheme(m_currentThemeId, path)) {
        ui::widgets::StyledMessageBox::information(this, "Export", "Theme exported successfully.");
    } else {
        ui::widgets::StyledMessageBox::warning(this, "Export", "Failed to export theme.");
    }
}

void SessionSettingsDialog::onApply() {
    emit applyRequested(collectThemeFromUI());
}

void SessionSettingsDialog::onApplyToAll() {
    emit applyToAllRequested(collectThemeFromUI());
}

void SessionSettingsDialog::onOk() {
    emit applyRequested(collectThemeFromUI());
    accept();
}

void SessionSettingsDialog::onMonochromeToggled(bool checked) {
    m_monochromeColorSwatch->setEnabled(checked);
    // Disable individual color swatches when monochrome is on
    m_colorGreen->setEnabled(!checked);
    m_colorWhite->setEnabled(!checked);
    m_colorBlue->setEnabled(!checked);
    m_colorYellow->setEnabled(!checked);
    m_colorRed->setEnabled(!checked);
    m_colorCyan->setEnabled(!checked);
    m_colorPink->setEnabled(!checked);
    m_colorBlack->setEnabled(!checked);
    m_colorCursor->setEnabled(!checked);
    onThemePropertyChanged();
}

void SessionSettingsDialog::onThemeFilterChanged(const QString &filter) {
    Q_UNUSED(filter);
    populateThemeDropdown();
}

void SessionSettingsDialog::onRandomizeColors() {
    auto *rng = QRandomGenerator::global();
    auto randColor = [&]() {
        return QColor::fromHsv(rng->bounded(360), 150 + rng->bounded(106), 150 + rng->bounded(106));
    };

    setSwatchColor(m_colorGreen,  randColor());
    setSwatchColor(m_colorWhite,  randColor());
    setSwatchColor(m_colorBlue,   randColor());
    setSwatchColor(m_colorYellow, randColor());
    setSwatchColor(m_colorRed,    randColor());
    setSwatchColor(m_colorCyan,   randColor());
    setSwatchColor(m_colorPink,   randColor());
    setSwatchColor(m_colorBlack,  QColor::fromHsv(rng->bounded(360), rng->bounded(50), rng->bounded(60)));
    setSwatchColor(m_colorCursor, randColor());

    // Randomize background to a dark color
    setSwatchColor(m_bgColorSwatch, QColor::fromHsv(rng->bounded(360), rng->bounded(80), rng->bounded(40)));

    onThemePropertyChanged();
}

double SessionSettingsDialog::wcagContrastRatio(const QColor &fg, const QColor &bg) {
    // WCAG 2.0 relative luminance
    auto srgbToLinear = [](double c) -> double {
        return (c <= 0.03928) ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    auto luminance = [&](const QColor &color) -> double {
        double r = srgbToLinear(color.redF());
        double g = srgbToLinear(color.greenF());
        double b = srgbToLinear(color.blueF());
        return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    };

    double l1 = luminance(fg);
    double l2 = luminance(bg);
    if (l2 > l1) std::swap(l1, l2);
    return (l1 + 0.05) / (l2 + 0.05);
}

void SessionSettingsDialog::updateContrastLabels() {
    QColor bg = swatchColor(m_bgColorSwatch);

    for (auto it = m_contrastLabels.begin(); it != m_contrastLabels.end(); ++it) {
        QPushButton *swatch = it.key();
        QLabel *label = it.value();
        QColor fg = swatchColor(swatch);
        double ratio = wcagContrastRatio(fg, bg);
        QString grade;
        if (ratio >= 7.0)      grade = "AAA";
        else if (ratio >= 4.5) grade = "AA";
        else if (ratio >= 3.0) grade = "A";
        else                   grade = "Fail";

        QString color = (ratio >= 4.5) ? "#4a4" : (ratio >= 3.0) ? "#aa4" : "#a44";
        label->setText(QString("%1:1 %2").arg(ratio, 0, 'f', 1).arg(grade));
        label->setStyleSheet(QString("color: %1; font-size: 10px;").arg(color));
    }
}
