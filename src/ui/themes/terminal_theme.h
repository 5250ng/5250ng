#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <functional>

namespace ui::themes {

struct TerminalTheme {
    // Identity
    QString id;
    QString displayName;
    QString description;
    bool builtin = false;

    // Theme inheritance: unset values fall through to parent
    QString parentThemeId;

    // Background
    enum BackgroundMode { Color, Image };
    BackgroundMode backgroundMode = Color;
    QColor backgroundColor = QColor(0, 0, 0);
    QString backgroundImagePath;
    QByteArray backgroundImageData; // base64-embedded image for .5250theme export
    enum BackgroundImageLayout { Stretch, Tile, Center, Fit };
    BackgroundImageLayout backgroundImageLayout = Stretch;
    double backgroundImageOpacity = 1.0;

    // Font
    QString fontFamily = "IBM 3270";
    int fontSize = 14;

    // 5250 protocol colors (mapped into 16-color scheme)
    QColor colorBlack  = QColor(0, 0, 0);
    QColor colorBlue   = QColor(0, 0, 255);
    QColor colorGreen  = QColor(0, 255, 0);
    QColor colorCyan   = QColor(0, 255, 255);
    QColor colorRed    = QColor(255, 0, 0);
    QColor colorPink   = QColor(255, 0, 255);
    QColor colorYellow = QColor(255, 255, 0);
    QColor colorWhite  = QColor(255, 255, 255);

    // Selection & highlight
    QColor selectionBackground = QColor(255, 255, 0, 64);
    QColor selectionForeground = QColor(255, 255, 255);
    QColor fieldIndicatorColor = QColor(0, 128, 255, 64);

    // Column separator
    QColor columnSeparatorColor = QColor(128, 128, 128);
    enum ColSepStyle { Solid, Dotted, Dimmed };
    ColSepStyle columnSeparatorStyle = Solid;

    // Cursor
    QColor cursorColor = QColor(0, 255, 0);
    enum CursorShape { Block, Underline, Bar };
    CursorShape cursorShape = Block;
    int cursorBlinkRateMs = 530;

    // CRT effect
    bool crtEffectEnabled = false;
    double crtScanlineIntensity = 0.3;
    double crtGlowRadius = 0.2;
    double crtCurvature = 0.1;

    // Global brightness/saturation adjustment
    double globalBrightness = 1.0;  // 0.5–1.5
    double globalSaturation = 1.0;  // 0.0–2.0

    // Build a full 16-color scheme vector from the 8 protocol colors.
    // Indices 0-7 are dim variants, 8-15 are bright variants.
    QVector<QColor> buildColorScheme() const;

    // Apply globalBrightness and globalSaturation to a color
    QColor adjustColor(const QColor &color) const;

    // Serialization
    QJsonObject toJson() const;
    static TerminalTheme fromJson(const QJsonObject &json);

    // Resolve inheritance: merge with parent theme chain.
    // The lookup function retrieves a theme by id.
    TerminalTheme resolved(const std::function<TerminalTheme(const QString &)> &lookup,
                           int maxDepth = 5) const;

    // String conversion helpers
    static QString backgroundModeToString(BackgroundMode mode);
    static BackgroundMode backgroundModeFromString(const QString &str);
    static QString imageLayoutToString(BackgroundImageLayout layout);
    static BackgroundImageLayout imageLayoutFromString(const QString &str);
    static QString colSepStyleToString(ColSepStyle style);
    static ColSepStyle colSepStyleFromString(const QString &str);
    static QString cursorShapeToString(CursorShape shape);
    static CursorShape cursorShapeFromString(const QString &str);
};

} // namespace ui::themes
