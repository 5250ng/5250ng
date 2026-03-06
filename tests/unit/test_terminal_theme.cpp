#include "ui/themes/terminal_theme.h"
#include <QJsonDocument>
#include <QtTest/QtTest>

using namespace ui::themes;

class TestTerminalTheme : public QObject {
    Q_OBJECT

  private slots:
    // String conversion roundtrips
    void testBackgroundModeStrings();
    void testImageLayoutStrings();
    void testColSepStyleStrings();
    void testCursorShapeStrings();
    void testGridModeStrings();
    void testStringConversionDefaults();

    // Serialization
    void testSerializationRoundtrip();
    void testSerializationOptionalFields();
    void testDeserializationDefaults();
    void testDeserializationMissingObjects();

    // Color scheme
    void testBuildColorSchemeSize();
    void testBuildColorSchemeValues();
    void testMonochromeColorScheme();
    void testMonochromeDisabled();

    // Color adjustment
    void testAdjustColorIdentity();
    void testAdjustColorBrightness();

    // Theme inheritance
    void testResolvedNoParent();
    void testResolvedWithParent();
    void testResolvedMissingParent();
    void testResolvedMaxDepth();
    void testResolvedPreservesDescription();
};

// --- String conversion roundtrips ---

void TestTerminalTheme::testBackgroundModeStrings() {
    QCOMPARE(TerminalTheme::backgroundModeToString(TerminalTheme::Color), QString("color"));
    QCOMPARE(TerminalTheme::backgroundModeToString(TerminalTheme::Image), QString("image"));
    QCOMPARE(TerminalTheme::backgroundModeFromString("color"), TerminalTheme::Color);
    QCOMPARE(TerminalTheme::backgroundModeFromString("image"), TerminalTheme::Image);
    // Roundtrip
    QCOMPARE(TerminalTheme::backgroundModeFromString(
                 TerminalTheme::backgroundModeToString(TerminalTheme::Image)),
             TerminalTheme::Image);
}

void TestTerminalTheme::testImageLayoutStrings() {
    QCOMPARE(TerminalTheme::imageLayoutToString(TerminalTheme::Stretch), QString("stretch"));
    QCOMPARE(TerminalTheme::imageLayoutToString(TerminalTheme::Tile), QString("tile"));
    QCOMPARE(TerminalTheme::imageLayoutToString(TerminalTheme::Center), QString("center"));
    QCOMPARE(TerminalTheme::imageLayoutToString(TerminalTheme::Fit), QString("fit"));
    QCOMPARE(TerminalTheme::imageLayoutFromString("tile"), TerminalTheme::Tile);
    QCOMPARE(TerminalTheme::imageLayoutFromString("center"), TerminalTheme::Center);
    QCOMPARE(TerminalTheme::imageLayoutFromString("fit"), TerminalTheme::Fit);
    QCOMPARE(TerminalTheme::imageLayoutFromString("stretch"), TerminalTheme::Stretch);
}

void TestTerminalTheme::testColSepStyleStrings() {
    QCOMPARE(TerminalTheme::colSepStyleToString(TerminalTheme::Solid), QString("solid"));
    QCOMPARE(TerminalTheme::colSepStyleToString(TerminalTheme::Dotted), QString("dotted"));
    QCOMPARE(TerminalTheme::colSepStyleToString(TerminalTheme::Dimmed), QString("dimmed"));
    QCOMPARE(TerminalTheme::colSepStyleFromString("dotted"), TerminalTheme::Dotted);
    QCOMPARE(TerminalTheme::colSepStyleFromString("dimmed"), TerminalTheme::Dimmed);
    QCOMPARE(TerminalTheme::colSepStyleFromString("solid"), TerminalTheme::Solid);
}

void TestTerminalTheme::testCursorShapeStrings() {
    QCOMPARE(TerminalTheme::cursorShapeToString(TerminalTheme::Block), QString("block"));
    QCOMPARE(TerminalTheme::cursorShapeToString(TerminalTheme::Underline), QString("underline"));
    QCOMPARE(TerminalTheme::cursorShapeToString(TerminalTheme::Bar), QString("bar"));
    QCOMPARE(TerminalTheme::cursorShapeFromString("underline"), TerminalTheme::Underline);
    QCOMPARE(TerminalTheme::cursorShapeFromString("bar"), TerminalTheme::Bar);
    QCOMPARE(TerminalTheme::cursorShapeFromString("block"), TerminalTheme::Block);
}

void TestTerminalTheme::testGridModeStrings() {
    QCOMPARE(TerminalTheme::gridModeToString(TerminalTheme::Packed), QString("packed"));
    QCOMPARE(TerminalTheme::gridModeToString(TerminalTheme::Wide), QString("wide"));
    QCOMPARE(TerminalTheme::gridModeFromString("wide"), TerminalTheme::Wide);
    QCOMPARE(TerminalTheme::gridModeFromString("packed"), TerminalTheme::Packed);
}

void TestTerminalTheme::testStringConversionDefaults() {
    // Invalid strings should return defaults
    QCOMPARE(TerminalTheme::backgroundModeFromString("invalid"), TerminalTheme::Color);
    QCOMPARE(TerminalTheme::imageLayoutFromString(""), TerminalTheme::Stretch);
    QCOMPARE(TerminalTheme::colSepStyleFromString("xyz"), TerminalTheme::Solid);
    QCOMPARE(TerminalTheme::cursorShapeFromString(""), TerminalTheme::Block);
    QCOMPARE(TerminalTheme::gridModeFromString("xxx"), TerminalTheme::Packed);
}

// --- Serialization ---

void TestTerminalTheme::testSerializationRoundtrip() {
    TerminalTheme original;
    original.id = "test_theme";
    original.displayName = "Test Theme";
    original.description = "A test description";
    original.builtin = false;
    original.backgroundColor = QColor(30, 30, 30);
    original.colorGreen = QColor(0, 200, 0);
    original.colorBlue = QColor(50, 50, 255);
    original.colorRed = QColor(255, 50, 50);
    original.colorWhite = QColor(220, 220, 220);
    original.colorCyan = QColor(0, 200, 200);
    original.colorYellow = QColor(200, 200, 0);
    original.colorPink = QColor(200, 0, 200);
    original.colorBlack = QColor(10, 10, 10);
    original.cursorColor = QColor(0, 255, 0);
    original.fontFamily = "IBM 3270";
    original.fontSize = 16;
    original.cursorShape = TerminalTheme::Underline;
    original.cursorBlinkRateMs = 600;
    original.columnSeparatorStyle = TerminalTheme::Dotted;
    original.crtEffectEnabled = true;
    original.crtScanlineIntensity = 0.5;
    original.crtGlowRadius = 0.3;
    original.crtCurvature = 0.15;
    original.globalBrightness = 1.1;
    original.globalSaturation = 0.9;
    original.gridMode = TerminalTheme::Wide;
    original.monochrome = true;
    original.monochromeColor = QColor(255, 176, 0);

    QJsonObject json = original.toJson();
    TerminalTheme restored = TerminalTheme::fromJson(json);

    QCOMPARE(restored.id, original.id);
    QCOMPARE(restored.displayName, original.displayName);
    QCOMPARE(restored.description, original.description);
    QCOMPARE(restored.builtin, original.builtin);
    QCOMPARE(restored.backgroundColor, original.backgroundColor);
    QCOMPARE(restored.colorGreen, original.colorGreen);
    QCOMPARE(restored.colorBlue, original.colorBlue);
    QCOMPARE(restored.colorRed, original.colorRed);
    QCOMPARE(restored.fontFamily, original.fontFamily);
    QCOMPARE(restored.fontSize, original.fontSize);
    QCOMPARE(restored.cursorShape, original.cursorShape);
    QCOMPARE(restored.cursorBlinkRateMs, original.cursorBlinkRateMs);
    QCOMPARE(restored.columnSeparatorStyle, original.columnSeparatorStyle);
    QCOMPARE(restored.crtEffectEnabled, original.crtEffectEnabled);
    QVERIFY(qFuzzyCompare(restored.crtScanlineIntensity, original.crtScanlineIntensity));
    QVERIFY(qFuzzyCompare(restored.globalBrightness, original.globalBrightness));
    QVERIFY(qFuzzyCompare(restored.globalSaturation, original.globalSaturation));
    QCOMPARE(restored.gridMode, original.gridMode);
    QCOMPARE(restored.monochrome, original.monochrome);
    QCOMPARE(restored.monochromeColor, original.monochromeColor);
}

void TestTerminalTheme::testSerializationOptionalFields() {
    // Description and parentThemeId should only appear in JSON when non-empty
    TerminalTheme t;
    t.id = "minimal";
    t.displayName = "Minimal";

    QJsonObject json = t.toJson();
    QVERIFY(!json.contains("description"));
    QVERIFY(!json.contains("parentThemeId"));
    QVERIFY(!json.contains("monochrome")); // not enabled

    // With description set
    t.description = "Has desc";
    json = t.toJson();
    QVERIFY(json.contains("description"));
    QCOMPARE(json["description"].toString(), QString("Has desc"));

    // With parentThemeId set
    t.parentThemeId = "parent_id";
    json = t.toJson();
    QVERIFY(json.contains("parentThemeId"));

    // With monochrome enabled
    t.monochrome = true;
    json = t.toJson();
    QVERIFY(json.contains("monochrome"));
    QVERIFY(json["monochrome"].toObject()["enabled"].toBool());
}

void TestTerminalTheme::testDeserializationDefaults() {
    // Minimal JSON — all optional fields should get defaults
    QJsonObject json;
    json["id"] = "test";

    TerminalTheme t = TerminalTheme::fromJson(json);
    QCOMPARE(t.id, QString("test"));
    QCOMPARE(t.displayName, QString("test")); // falls back to id
    QCOMPARE(t.description, QString());
    QCOMPARE(t.builtin, false);
    QCOMPARE(t.backgroundMode, TerminalTheme::Color);
    QCOMPARE(t.fontSize, 14);
    QCOMPARE(t.cursorShape, TerminalTheme::Block);
    QCOMPARE(t.cursorBlinkRateMs, 530);
    QCOMPARE(t.crtEffectEnabled, false);
    QCOMPARE(t.gridMode, TerminalTheme::Packed);
    QCOMPARE(t.monochrome, false);
    QVERIFY(qFuzzyCompare(t.globalBrightness, 1.0));
    QVERIFY(qFuzzyCompare(t.globalSaturation, 1.0));
}

void TestTerminalTheme::testDeserializationMissingObjects() {
    // JSON with no nested objects at all
    QJsonObject json;
    json["id"] = "bare";
    json["displayName"] = "Bare Theme";

    TerminalTheme t = TerminalTheme::fromJson(json);
    QCOMPARE(t.id, QString("bare"));
    // Colors should be defaults since "colors" object is missing
    QCOMPARE(t.colorGreen, QColor(0, 255, 0));
    QCOMPARE(t.colorBlue, QColor(0, 0, 255));
    QCOMPARE(t.backgroundColor, QColor(0, 0, 0));
}

// --- Color scheme ---

void TestTerminalTheme::testBuildColorSchemeSize() {
    TerminalTheme t;
    QVector<QColor> scheme = t.buildColorScheme();
    QCOMPARE(scheme.size(), 16);
    // All colors should be valid
    for (int i = 0; i < 16; ++i) {
        QVERIFY2(scheme[i].isValid(), qPrintable(QString("Color %1 invalid").arg(i)));
    }
}

void TestTerminalTheme::testBuildColorSchemeValues() {
    TerminalTheme t;
    t.globalBrightness = 1.0;
    t.globalSaturation = 1.0;
    QVector<QColor> scheme = t.buildColorScheme();

    // Index 10 = bright green (should match colorGreen with adjustColor identity)
    QCOMPARE(scheme[10], t.colorGreen);
    // Index 9 = bright blue
    QCOMPARE(scheme[9], t.colorBlue);
    // Index 15 = bright white
    QCOMPARE(scheme[15], t.colorWhite);
}

void TestTerminalTheme::testMonochromeColorScheme() {
    TerminalTheme t;
    t.monochrome = true;
    t.monochromeColor = QColor(255, 176, 0); // amber
    t.backgroundColor = QColor(26, 8, 0);
    t.globalBrightness = 1.0;
    t.globalSaturation = 1.0;

    QVector<QColor> scheme = t.buildColorScheme();
    QCOMPARE(scheme.size(), 16);

    // Index 10 (green slot) should be the base monochrome color
    QCOMPARE(scheme[10], t.monochromeColor);

    // All non-background colors should share the same hue
    int baseHue = t.monochromeColor.hsvHue();
    for (int i = 1; i < 16; ++i) { // skip 0 (background)
        int hue = scheme[i].hsvHue();
        // Hue should be the same (allow small rounding differences)
        QVERIFY2(qAbs(hue - baseHue) <= 1 || qAbs(hue - baseHue) >= 359,
                 qPrintable(QString("Color %1 hue %2 != base hue %3").arg(i).arg(hue).arg(baseHue)));
    }
}

void TestTerminalTheme::testMonochromeDisabled() {
    TerminalTheme t;
    t.monochrome = false;
    t.monochromeColor = QColor(255, 176, 0);
    t.colorGreen = QColor(0, 255, 0);
    t.colorBlue = QColor(0, 0, 255);
    t.globalBrightness = 1.0;
    t.globalSaturation = 1.0;

    QVector<QColor> scheme = t.buildColorScheme();
    // When monochrome is off, individual colors should be used
    QCOMPARE(scheme[10], t.colorGreen);
    QCOMPARE(scheme[9], t.colorBlue);
}

// --- Color adjustment ---

void TestTerminalTheme::testAdjustColorIdentity() {
    TerminalTheme t;
    t.globalBrightness = 1.0;
    t.globalSaturation = 1.0;

    QColor input(128, 64, 200);
    QColor output = t.adjustColor(input);
    QCOMPARE(output, input);
}

void TestTerminalTheme::testAdjustColorBrightness() {
    TerminalTheme t;
    t.globalBrightness = 0.5;
    t.globalSaturation = 1.0;

    QColor bright(200, 200, 200);
    QColor dimmed = t.adjustColor(bright);
    // Dimmed should be darker (lower lightness)
    QVERIFY(dimmed.lightnessF() < bright.lightnessF());
}

// --- Theme inheritance ---

void TestTerminalTheme::testResolvedNoParent() {
    TerminalTheme t;
    t.id = "standalone";
    t.displayName = "Standalone";
    t.colorGreen = QColor(0, 200, 0);

    TerminalTheme resolved = t.resolved([](const QString &) { return TerminalTheme{}; });
    QCOMPARE(resolved.id, t.id);
    QCOMPARE(resolved.colorGreen, t.colorGreen);
}

void TestTerminalTheme::testResolvedWithParent() {
    TerminalTheme parent;
    parent.id = "parent";
    parent.displayName = "Parent";
    parent.colorGreen = QColor(0, 128, 0);
    parent.fontFamily = "Courier";
    parent.fontSize = 12;

    TerminalTheme child;
    child.id = "child";
    child.displayName = "Child";
    child.description = "Child desc";
    child.parentThemeId = "parent";
    child.colorGreen = QColor(0, 255, 0);
    child.fontFamily = "IBM 3270";
    child.fontSize = 16;

    auto lookup = [&](const QString &id) -> TerminalTheme {
        if (id == "parent") return parent;
        return TerminalTheme{};
    };

    TerminalTheme resolved = child.resolved(lookup);
    // Child values should override parent
    QCOMPARE(resolved.id, QString("child"));
    QCOMPARE(resolved.displayName, QString("Child"));
    QCOMPARE(resolved.colorGreen, QColor(0, 255, 0));
    QCOMPARE(resolved.fontFamily, QString("IBM 3270"));
    QCOMPARE(resolved.fontSize, 16);
}

void TestTerminalTheme::testResolvedMissingParent() {
    TerminalTheme child;
    child.id = "orphan";
    child.parentThemeId = "nonexistent";
    child.colorGreen = QColor(0, 255, 0);

    auto lookup = [](const QString &) { return TerminalTheme{}; };
    TerminalTheme resolved = child.resolved(lookup);
    // Should return self when parent not found
    QCOMPARE(resolved.id, QString("orphan"));
    QCOMPARE(resolved.colorGreen, QColor(0, 255, 0));
}

void TestTerminalTheme::testResolvedMaxDepth() {
    // Create circular reference — should not infinite loop
    TerminalTheme a;
    a.id = "a";
    a.parentThemeId = "b";
    a.colorGreen = QColor(255, 0, 0);

    TerminalTheme b;
    b.id = "b";
    b.parentThemeId = "a";
    b.colorGreen = QColor(0, 0, 255);

    auto lookup = [&](const QString &id) -> TerminalTheme {
        if (id == "a") return a;
        if (id == "b") return b;
        return TerminalTheme{};
    };

    // Should terminate without hanging
    TerminalTheme resolved = a.resolved(lookup, 5);
    QCOMPARE(resolved.id, QString("a"));
}

void TestTerminalTheme::testResolvedPreservesDescription() {
    TerminalTheme parent;
    parent.id = "parent";
    parent.description = "Parent description";

    TerminalTheme child;
    child.id = "child";
    child.description = "Child description";
    child.parentThemeId = "parent";

    auto lookup = [&](const QString &id) -> TerminalTheme {
        if (id == "parent") return parent;
        return TerminalTheme{};
    };

    TerminalTheme resolved = child.resolved(lookup);
    QCOMPARE(resolved.description, QString("Child description"));
}

QTEST_MAIN(TestTerminalTheme)
#include "test_terminal_theme.moc"
