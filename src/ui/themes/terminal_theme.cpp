#include "terminal_theme.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>

namespace ui::themes {

// --- String conversion helpers ---

QString TerminalTheme::backgroundModeToString(BackgroundMode mode) {
    switch (mode) {
    case Color: return "color";
    case Image: return "image";
    }
    return "color";
}

TerminalTheme::BackgroundMode TerminalTheme::backgroundModeFromString(const QString &str) {
    if (str == "image") return Image;
    return Color;
}

QString TerminalTheme::imageLayoutToString(BackgroundImageLayout layout) {
    switch (layout) {
    case Stretch: return "stretch";
    case Tile:    return "tile";
    case Center:  return "center";
    case Fit:     return "fit";
    }
    return "stretch";
}

TerminalTheme::BackgroundImageLayout TerminalTheme::imageLayoutFromString(const QString &str) {
    if (str == "tile")   return Tile;
    if (str == "center") return Center;
    if (str == "fit")    return Fit;
    return Stretch;
}

QString TerminalTheme::colSepStyleToString(ColSepStyle style) {
    switch (style) {
    case Solid:  return "solid";
    case Dotted: return "dotted";
    case Dimmed: return "dimmed";
    }
    return "solid";
}

TerminalTheme::ColSepStyle TerminalTheme::colSepStyleFromString(const QString &str) {
    if (str == "dotted") return Dotted;
    if (str == "dimmed") return Dimmed;
    return Solid;
}

QString TerminalTheme::cursorShapeToString(CursorShape shape) {
    switch (shape) {
    case Block:     return "block";
    case Underline: return "underline";
    case Bar:       return "bar";
    }
    return "block";
}

TerminalTheme::CursorShape TerminalTheme::cursorShapeFromString(const QString &str) {
    if (str == "underline") return Underline;
    if (str == "bar")       return Bar;
    return Block;
}

QString TerminalTheme::gridModeToString(GridMode mode) {
    switch (mode) {
    case Packed: return "packed";
    case Wide:   return "wide";
    }
    return "packed";
}

TerminalTheme::GridMode TerminalTheme::gridModeFromString(const QString &str) {
    if (str == "wide") return Wide;
    return Packed;
}

// --- Color adjustment ---

QColor TerminalTheme::adjustColor(const QColor &color) const {
    if (qFuzzyCompare(globalBrightness, 1.0) && qFuzzyCompare(globalSaturation, 1.0)) {
        return color;
    }

    float h, s, l, a;
    color.getHslF(&h, &s, &l, &a);

    // Apply saturation multiplier
    s = qBound(0.0f, s * static_cast<float>(globalSaturation), 1.0f);

    // Apply brightness multiplier
    l = qBound(0.0f, l * static_cast<float>(globalBrightness), 1.0f);

    QColor result;
    result.setHslF(h, s, l, a);
    return result;
}

// --- Build 16-color scheme ---

QVector<QColor> TerminalTheme::buildColorScheme() const {
    QVector<QColor> scheme(16);

    if (monochrome && monochromeColor.isValid()) {
        // Monochrome: all colors are brightness variants of the base hue
        int h, s, v;
        monochromeColor.getHsv(&h, &s, &v);

        // Dim variants (indices 0-7)
        scheme[0]  = adjustColor(backgroundColor);                            // black
        scheme[1]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 50 / 100, 255)));  // dim blue
        scheme[2]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 60 / 100, 255)));  // dim green
        scheme[3]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 55 / 100, 255)));  // dim cyan
        scheme[4]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 45 / 100, 255)));  // dim red
        scheme[5]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 50 / 100, 255)));  // dim pink
        scheme[6]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 40 / 100, 255)));  // brown
        scheme[7]  = adjustColor(QColor::fromHsv(h, s * 70 / 100, qBound(0, v * 75 / 100, 255))); // light gray

        // Bright variants (indices 8-15)
        scheme[8]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 35 / 100, 255)));  // dark gray
        scheme[9]  = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 70 / 100, 255)));  // blue
        scheme[10] = adjustColor(monochromeColor);                                        // green (base)
        scheme[11] = adjustColor(QColor::fromHsv(h, s * 80 / 100, qBound(0, v * 90 / 100, 255))); // cyan
        scheme[12] = adjustColor(QColor::fromHsv(h, s, qBound(0, v * 60 / 100, 255)));  // red
        scheme[13] = adjustColor(QColor::fromHsv(h, s * 85 / 100, qBound(0, v * 80 / 100, 255))); // pink
        scheme[14] = adjustColor(QColor::fromHsv(h, s * 70 / 100, qMin(255, v * 110 / 100))); // yellow
        scheme[15] = adjustColor(QColor::fromHsv(h, s * 50 / 100, qMin(255, v * 120 / 100))); // white

        return scheme;
    }

    // Dim variants (indices 0-7)
    scheme[0]  = adjustColor(colorBlack);
    scheme[1]  = adjustColor(colorBlue.darker(150));
    scheme[2]  = adjustColor(colorGreen.darker(150));
    scheme[3]  = adjustColor(colorCyan.darker(150));
    scheme[4]  = adjustColor(colorRed.darker(150));
    scheme[5]  = adjustColor(colorPink.darker(150));
    scheme[6]  = adjustColor(QColor(128, 64, 0)); // Brown (dim yellow)
    scheme[7]  = adjustColor(QColor(192, 192, 192)); // Light gray

    // Bright variants (indices 8-15) — these are the ones the 5250 attribute table uses
    scheme[8]  = adjustColor(QColor(128, 128, 128)); // Dark gray
    scheme[9]  = adjustColor(colorBlue);
    scheme[10] = adjustColor(colorGreen);
    scheme[11] = adjustColor(colorCyan);
    scheme[12] = adjustColor(colorRed);
    scheme[13] = adjustColor(colorPink);
    scheme[14] = adjustColor(colorYellow);
    scheme[15] = adjustColor(colorWhite);

    return scheme;
}

// --- Serialization ---

static QString colorToHex(const QColor &c) {
    if (c.alpha() == 255) {
        return c.name(QColor::HexRgb);
    }
    return c.name(QColor::HexArgb);
}

static QColor colorFromHex(const QString &str, const QColor &fallback = Qt::black) {
    QColor c(str);
    return c.isValid() ? c : fallback;
}

QJsonObject TerminalTheme::toJson() const {
    QJsonObject json;
    json["id"] = id;
    json["displayName"] = displayName;
    if (!description.isEmpty()) {
        json["description"] = description;
    }
    json["builtin"] = builtin;

    if (!parentThemeId.isEmpty()) {
        json["parentThemeId"] = parentThemeId;
    }

    // Background
    QJsonObject bg;
    bg["mode"] = backgroundModeToString(backgroundMode);
    bg["color"] = colorToHex(backgroundColor);
    if (!backgroundImagePath.isEmpty()) {
        bg["imagePath"] = backgroundImagePath;
    }
    if (!backgroundImageData.isEmpty()) {
        bg["imageData"] = QString::fromLatin1(backgroundImageData.toBase64());
    }
    bg["imageLayout"] = imageLayoutToString(backgroundImageLayout);
    bg["imageOpacity"] = backgroundImageOpacity;
    json["background"] = bg;

    // Font
    QJsonObject fontObj;
    fontObj["family"] = fontFamily;
    fontObj["size"] = fontSize;
    json["font"] = fontObj;

    // Colors
    QJsonObject colors;
    colors["black"]  = colorToHex(colorBlack);
    colors["blue"]   = colorToHex(colorBlue);
    colors["green"]  = colorToHex(colorGreen);
    colors["cyan"]   = colorToHex(colorCyan);
    colors["red"]    = colorToHex(colorRed);
    colors["pink"]   = colorToHex(colorPink);
    colors["yellow"] = colorToHex(colorYellow);
    colors["white"]  = colorToHex(colorWhite);
    colors["cursor"] = colorToHex(cursorColor);
    json["colors"] = colors;

    // Selection & indicators
    QJsonObject selection;
    selection["background"]     = colorToHex(selectionBackground);
    selection["foreground"]     = colorToHex(selectionForeground);
    selection["fieldIndicator"] = colorToHex(fieldIndicatorColor);
    json["selection"] = selection;

    // Column separator
    QJsonObject colSep;
    colSep["color"] = colorToHex(columnSeparatorColor);
    colSep["style"] = colSepStyleToString(columnSeparatorStyle);
    json["columnSeparator"] = colSep;

    // Cursor
    QJsonObject cursor;
    cursor["shape"]    = cursorShapeToString(cursorShape);
    cursor["blinkMs"]  = cursorBlinkRateMs;
    json["cursor"] = cursor;

    // CRT effect
    QJsonObject crt;
    crt["enabled"]            = crtEffectEnabled;
    crt["scanlineIntensity"]  = crtScanlineIntensity;
    crt["glowRadius"]         = crtGlowRadius;
    crt["curvature"]          = crtCurvature;
    crt["phosphorBloom"]      = crtPhosphorBloom;
    json["crtEffect"] = crt;

    // Grid mode
    json["gridMode"] = gridModeToString(gridMode);

    // Monochrome
    if (monochrome) {
        QJsonObject mono;
        mono["enabled"] = true;
        mono["color"] = colorToHex(monochromeColor);
        json["monochrome"] = mono;
    }

    // Global adjustments
    json["globalBrightness"]  = globalBrightness;
    json["globalSaturation"]  = globalSaturation;

    return json;
}

TerminalTheme TerminalTheme::fromJson(const QJsonObject &json) {
    TerminalTheme t;

    t.id          = json["id"].toString();
    t.displayName = json["displayName"].toString(t.id);
    t.description = json["description"].toString();
    t.builtin     = json["builtin"].toBool(false);
    t.parentThemeId = json["parentThemeId"].toString();

    // Background
    if (json.contains("background") && json["background"].isObject()) {
        QJsonObject bg = json["background"].toObject();
        t.backgroundMode         = backgroundModeFromString(bg["mode"].toString());
        t.backgroundColor        = colorFromHex(bg["color"].toString(), QColor(0, 0, 0));
        t.backgroundImagePath    = bg["imagePath"].toString();
        if (bg.contains("imageData")) {
            t.backgroundImageData = QByteArray::fromBase64(bg["imageData"].toString().toLatin1());
        }
        t.backgroundImageLayout  = imageLayoutFromString(bg["imageLayout"].toString());
        t.backgroundImageOpacity = bg["imageOpacity"].toDouble(1.0);
    }

    // Font
    if (json.contains("font") && json["font"].isObject()) {
        QJsonObject fontObj = json["font"].toObject();
        t.fontFamily = fontObj["family"].toString("Courier");
        t.fontSize   = fontObj["size"].toInt(14);
    }

    // Colors
    if (json.contains("colors") && json["colors"].isObject()) {
        QJsonObject c = json["colors"].toObject();
        t.colorBlack  = colorFromHex(c["black"].toString(),  QColor(0, 0, 0));
        t.colorBlue   = colorFromHex(c["blue"].toString(),   QColor(0, 0, 255));
        t.colorGreen  = colorFromHex(c["green"].toString(),  QColor(0, 255, 0));
        t.colorCyan   = colorFromHex(c["cyan"].toString(),   QColor(0, 255, 255));
        t.colorRed    = colorFromHex(c["red"].toString(),    QColor(255, 0, 0));
        t.colorPink   = colorFromHex(c["pink"].toString(),   QColor(255, 0, 255));
        t.colorYellow = colorFromHex(c["yellow"].toString(), QColor(255, 255, 0));
        t.colorWhite  = colorFromHex(c["white"].toString(),  QColor(255, 255, 255));
        t.cursorColor = colorFromHex(c["cursor"].toString(), t.colorGreen);
    }

    // Selection & indicators
    if (json.contains("selection") && json["selection"].isObject()) {
        QJsonObject sel = json["selection"].toObject();
        t.selectionBackground = colorFromHex(sel["background"].toString(), QColor(255, 255, 0, 64));
        t.selectionForeground = colorFromHex(sel["foreground"].toString(), QColor(255, 255, 255));
        t.fieldIndicatorColor = colorFromHex(sel["fieldIndicator"].toString(), QColor(0, 128, 255, 64));
    }

    // Column separator
    if (json.contains("columnSeparator") && json["columnSeparator"].isObject()) {
        QJsonObject cs = json["columnSeparator"].toObject();
        t.columnSeparatorColor = colorFromHex(cs["color"].toString(), QColor(128, 128, 128));
        t.columnSeparatorStyle = colSepStyleFromString(cs["style"].toString());
    }

    // Cursor
    if (json.contains("cursor") && json["cursor"].isObject()) {
        QJsonObject cur = json["cursor"].toObject();
        t.cursorShape       = cursorShapeFromString(cur["shape"].toString());
        t.cursorBlinkRateMs = cur["blinkMs"].toInt(530);
    }

    // CRT effect
    if (json.contains("crtEffect") && json["crtEffect"].isObject()) {
        QJsonObject crt = json["crtEffect"].toObject();
        t.crtEffectEnabled       = crt["enabled"].toBool(false);
        t.crtScanlineIntensity   = crt["scanlineIntensity"].toDouble(0.3);
        t.crtGlowRadius          = crt["glowRadius"].toDouble(0.2);
        t.crtCurvature           = crt["curvature"].toDouble(0.1);
        t.crtPhosphorBloom       = crt["phosphorBloom"].toDouble(0.0);
    }

    // Grid mode
    t.gridMode = gridModeFromString(json["gridMode"].toString());

    // Monochrome
    if (json.contains("monochrome") && json["monochrome"].isObject()) {
        QJsonObject mono = json["monochrome"].toObject();
        t.monochrome = mono["enabled"].toBool(false);
        t.monochromeColor = colorFromHex(mono["color"].toString(), QColor(255, 176, 0));
    }

    // Global adjustments
    t.globalBrightness = json["globalBrightness"].toDouble(1.0);
    t.globalSaturation = json["globalSaturation"].toDouble(1.0);

    return t;
}

// --- Theme inheritance ---

TerminalTheme TerminalTheme::resolved(
    const std::function<TerminalTheme(const QString &)> &lookup,
    int maxDepth) const {

    if (parentThemeId.isEmpty() || maxDepth <= 0) {
        return *this;
    }

    TerminalTheme parent = lookup(parentThemeId);
    if (parent.id.isEmpty()) {
        // Parent not found — return self as-is
        return *this;
    }

    // Recursively resolve the parent
    TerminalTheme resolvedParent = parent.resolved(lookup, maxDepth - 1);

    // Start with the resolved parent and overlay our values.
    // For simplicity, we overlay all explicitly-set fields.
    // In this implementation, *all* fields from the child override the parent
    // (a more granular approach would track which fields were explicitly set).
    TerminalTheme result = resolvedParent;

    // Always override identity
    result.id          = id;
    result.displayName = displayName;
    result.description = description;
    result.builtin     = builtin;
    result.parentThemeId = parentThemeId;

    // Override all visual properties from child
    result.backgroundMode         = backgroundMode;
    result.backgroundColor        = backgroundColor;
    result.backgroundImagePath    = backgroundImagePath;
    result.backgroundImageData    = backgroundImageData;
    result.backgroundImageLayout  = backgroundImageLayout;
    result.backgroundImageOpacity = backgroundImageOpacity;

    result.fontFamily = fontFamily;
    result.fontSize   = fontSize;

    result.colorBlack  = colorBlack;
    result.colorBlue   = colorBlue;
    result.colorGreen  = colorGreen;
    result.colorCyan   = colorCyan;
    result.colorRed    = colorRed;
    result.colorPink   = colorPink;
    result.colorYellow = colorYellow;
    result.colorWhite  = colorWhite;

    result.selectionBackground = selectionBackground;
    result.selectionForeground = selectionForeground;
    result.fieldIndicatorColor = fieldIndicatorColor;

    result.columnSeparatorColor = columnSeparatorColor;
    result.columnSeparatorStyle = columnSeparatorStyle;

    result.cursorColor      = cursorColor;
    result.cursorShape      = cursorShape;
    result.cursorBlinkRateMs = cursorBlinkRateMs;

    result.gridMode = gridMode;

    result.monochrome = monochrome;
    result.monochromeColor = monochromeColor;

    result.crtEffectEnabled     = crtEffectEnabled;
    result.crtScanlineIntensity = crtScanlineIntensity;
    result.crtGlowRadius        = crtGlowRadius;
    result.crtCurvature         = crtCurvature;
    result.crtPhosphorBloom     = crtPhosphorBloom;

    result.globalBrightness = globalBrightness;
    result.globalSaturation = globalSaturation;

    return result;
}

} // namespace ui::themes
