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
    double screenBackgroundOpacity = 1.0; // opacity of 5250 screen background (< 1 to see image through)

    // Font
    QString fontFamily = "IBM Plex Mono";
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
    bool columnSeparatorEnabled = true;
    QColor columnSeparatorColor = QColor(128, 128, 128);
    enum ColSepStyle { Solid, Dotted, Dimmed };
    ColSepStyle columnSeparatorStyle = Solid;

    // Cell grid overlay
    QColor cellGridColor = QColor(255, 255, 255, 40);

    // Cursor
    QColor cursorColor = QColor(0, 255, 0);
    enum CursorShape { Block, Underline, Bar };
    CursorShape cursorShape = Block;
    int cursorBlinkRateMs = 530;

    // Screen grid layout
    enum GridMode { Packed, Wide };
    GridMode gridMode = Packed;

    // CRT effect
    bool crtEffectEnabled = false;
    double crtScanlineIntensity = 0.3;
    double crtGlowRadius = 0.2;
    double crtCurvature = 0.1;
    double crtPhosphorBloom = 0.0; // local phosphor glow around characters (0.0–1.0)

    // Monochrome mode: derive all protocol colors from a single hue
    bool monochrome = false;
    QColor monochromeColor = QColor(255, 176, 0); // default amber

    // HRule color (horizontal rule between screen and footer).
    // Invalid (default-constructed) means "derive from colorGreen at runtime".
    QColor hruleColor;

    // Footer indicator colors (status bar below the terminal screen).
    // Invalid (default-constructed) means "derive from protocol colors at runtime".
    QColor footerKbdStateColor;     // keyboard state (KBD LOCKED, ERROR, INS, …)
    QColor footerSystemNameColor;   // detected system name
    QColor footerHistoryColor;      // screen history position
    QColor footerMacroColor;        // macro recording/playback
    QColor footerCoordinatesColor;  // cursor row/col

    // Resolve a footer color: return the explicit value if valid, else the fallback.
    static QColor resolveFooterColor(const QColor &explicit_, const QColor &fallback) {
        return explicit_.isValid() ? explicit_ : fallback;
    }

    // Global brightness/saturation adjustment
    double globalBrightness = 1.0;  // 0.0–2.0
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
    static QString gridModeToString(GridMode mode);
    static GridMode gridModeFromString(const QString &str);
};

} // namespace ui::themes
