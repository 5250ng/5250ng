// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "ui/rendering/gddm_5292_decoder.h"

#include <QtTest/QtTest>

using ui::rendering::Gddm5292Decoder;

class TestGddm5292 : public QObject {
    Q_OBJECT

  private slots:
    void ignoresAlphanumericWrites();
    void decodesDocumentedPolylineFixture();
    void resumesVariableOrderAcrossBlocks();
    void systemResetClearsPlane();
    void suppressesPacingWhenRequested();
    void rejectsMalformedCoordinates();
    void decodesReadStatusOffset();
    void appliesIndexedRasterFunctions();
    void paletteChangesRecolorExistingPels();
    void reportsStructuredErrorStatus();
    void classifiesUndefinedAndInvalidSetOrders();
    void suppressesFatalErrorCompletionWhenRequested();
    void decodesCapturedGddmDemoBlock();
    void fillsCapturedSolidPolygon();
    void fillOnlyModeFillsCapturedTriangle();
    void boundaryOnlyModeLeavesInteriorClear();
    void appliesStyleToCapturedStarFill();
    void resumesFillPolygonAcrossBlocks();
    void rejectsFillPolygonOverEdgeLimit();
};

namespace {

// Device Y counts up from the bottom; the plane is addressed from the top.
int planeY(int deviceY) { return Gddm5292Decoder::kHeight - 1 - deviceY; }

// Encode one 10-bit coordinate as the device's two graphics-data bytes.
QByteArray coordinate(int value) {
    QByteArray out;
    out.append(static_cast<char>(0x40 | ((value >> 6) & 0x0F)));
    out.append(static_cast<char>(0x40 | (value & 0x3F)));
    return out;
}

QByteArray point(int x, int y) { return coordinate(x) + coordinate(y); }

} // namespace

void TestGddm5292::ignoresAlphanumericWrites() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("114040"));
    QVERIFY(!result.handled);
    QCOMPARE(decoder.blockCount(), quint64(0));
}

void TestGddm5292::decodesDocumentedPolylineFixture() {
    Gddm5292Decoder decoder;
    const QByteArray block = QByteArray::fromHex(
        "ff93b041a04040404a41644072425640599295");

    const auto result = decoder.process(block);

    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::Success);
    QVERIFY(result.changed);
    QVERIFY(decoder.displayEnabled());
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().size(), QSize(480, 288));
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 277), QColor(255, 0, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(100, 237), QColor(255, 0, 0));
}

void TestGddm5292::resumesVariableOrderAcrossBlocks() {
    Gddm5292Decoder decoder;
    const auto first = decoder.process(QByteArray::fromHex("ff93a04040404a416491"));
    QVERIFY(first.handled);
    QVERIFY(!first.error);
    QCOMPARE(first.completion, Gddm5292Decoder::Completion::Success);
    QVERIFY(decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 277), QColor(0, 0, 0));

    const auto second = decoder.process(QByteArray::fromHex("4072425640599295"));
    QVERIFY(second.handled);
    QVERIFY(!second.error);
    QVERIFY(second.changed);
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 277), QColor(255, 255, 255));
}

void TestGddm5292::systemResetClearsPlane() {
    Gddm5292Decoder decoder;
    decoder.process(QByteArray::fromHex("ff93a04040404a416440729295"));
    QVERIFY(decoder.displayEnabled());

    const auto reset = decoder.process(QByteArray::fromHex("ffff"));
    QVERIFY(reset.handled);
    QCOMPARE(reset.completion, Gddm5292Decoder::Completion::SystemReset);
    QVERIFY(!decoder.displayEnabled());
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 277), QColor(0, 0, 0));
}

void TestGddm5292::suppressesPacingWhenRequested() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ff9695"));
    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::None);
}

void TestGddm5292::rejectsMalformedCoordinates() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ffa04040404a92"));
    QVERIFY(result.handled);
    QVERIFY(result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::FatalError);
    QVERIFY(!decoder.graphicsMode());
}

void TestGddm5292::decodesReadStatusOffset() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ff805443959090909090"));
    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::Success);
    QCOMPARE(result.statusWrites.size(), 1);
    QCOMPARE(result.statusWrites[0].offset, 1283);
    QByteArray expectedStatus = QByteArray::fromHex("fffffff280ffff");
    expectedStatus.append(QByteArray(13, static_cast<char>(0x40)));
    QCOMPARE(result.statusWrites[0].data, expectedStatus);
    QCOMPARE(result.statusWrites[0].data.size(), 20);
    QVERIFY(!decoder.graphicsMode());
}

void TestGddm5292::appliesIndexedRasterFunctions() {
    Gddm5292Decoder decoder;
    const QByteArray point = QByteArray::fromHex("a0404040404040404092");

    decoder.process(QByteArray::fromHex("ff93b041b343") + point);
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(255, 0, 0));

    decoder.process(QByteArray::fromHex("b042b341") + point);
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(0, 0, 255));

    decoder.process(QByteArray::fromHex("b342") + point);
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(255, 0, 0));

    decoder.process(QByteArray::fromHex("b343") + point + QByteArray::fromHex("95"));
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(0, 255, 0));
}

void TestGddm5292::paletteChangesRecolorExistingPels() {
    Gddm5292Decoder decoder;
    decoder.process(QByteArray::fromHex(
        "ff93b041a0404040404040404092b4417f789295"));

    QCOMPARE(decoder.graphicsPlane().format(), QImage::Format_Indexed8);
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(255, 255, 255));
}

void TestGddm5292::reportsStructuredErrorStatus() {
    Gddm5292Decoder decoder;
    const auto malformed = decoder.process(QByteArray::fromHex("ffa04040404a92"));
    QVERIFY(malformed.error);
    QCOMPARE(malformed.completion, Gddm5292Decoder::Completion::FatalError);
    QCOMPARE(decoder.statusBytes().left(2), QByteArray::fromHex("c7f1"));
    QVERIFY(decoder.statusBytes().mid(5, 2) != QByteArray::fromHex("ffff"));
}

void TestGddm5292::classifiesUndefinedAndInvalidSetOrders() {
    Gddm5292Decoder undefinedOrder;
    const auto undefined = undefinedOrder.process(QByteArray::fromHex("ffa2"));
    QVERIFY(undefined.error);
    QCOMPARE(undefinedOrder.statusBytes().left(2), QByteArray::fromHex("c7f2"));

    Gddm5292Decoder invalidSet;
    const auto invalid = invalidSet.process(QByteArray::fromHex("ffb549"));
    QVERIFY(invalid.error);
    QCOMPARE(invalidSet.statusBytes().left(2), QByteArray::fromHex("c7f3"));
}

void TestGddm5292::suppressesFatalErrorCompletionWhenRequested() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ff96a2"));

    QVERIFY(result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::None);
    QCOMPARE(decoder.statusBytes().left(2), QByteArray::fromHex("c7f2"));
}

void TestGddm5292::decodesCapturedGddmDemoBlock() {
    Gddm5292Decoder decoder;
    const QByteArray block = QByteArray::fromHex(
        "ffb14f404f40b343b240a34093c3414642434342444545444641474892"
        "b4414078427840437878444740454778467f40477f7892b14f404f40b640"
        "b044b343a040404040466f444292"
        "9393939393939393939393939393939393939393939393939393939393939393"
        "9393939393939393939393939393939393939393939393939393939393939393"
        "9393939393939393939393939393939393939393939393939393939393939393"
        "939393939393939393939393939393939395");

    const auto result = decoder.process(block);
    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::Success);
    QVERIFY(result.changed);
    QVERIFY(decoder.displayEnabled());
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(0, 255, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(431, 29), QColor(0, 255, 0));
}

// The fill fixtures below are the exact bytes IBM i sent for a GSAREA variant
// of the shapes sample, taken from GDDM/captures/gddm-gsarea.pcap. Only the
// leading FF 93 and the trailing 95 are added to make each a complete block.
// No Set Color Table is included, so the device default palette applies.

void TestGddm5292::fillsCapturedSolidPolygon() {
    Gddm5292Decoder decoder;
    // Colour 2, solid style, fill mode 40/40 (bb=00, solid edge plus fill),
    // then a square spanning device x 57..167, y 34..100.
    const auto result = decoder.process(QByteArray::fromHex(
        "ff93b042b14f404f40b74040"
        "a54079406242674062426741644079416492"
        "95"));

    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QVERIFY(result.changed);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::Success);

    const QImage &plane = decoder.graphicsPlane();
    QCOMPARE(plane.pixelColor(112, planeY(67)), QColor(0, 255, 0));
    QCOMPARE(plane.pixelColor(60, planeY(37)), QColor(0, 255, 0));
    QCOMPARE(plane.pixelColor(164, planeY(97)), QColor(0, 255, 0));
    // Outside the polygon the plane keeps its cleared colour.
    QCOMPARE(plane.pixelColor(40, planeY(67)), QColor(0, 0, 0));
    QCOMPARE(plane.pixelColor(200, planeY(67)), QColor(0, 0, 0));
}

void TestGddm5292::fillOnlyModeFillsCapturedTriangle() {
    Gddm5292Decoder decoder;
    // Fill mode 42/40 is bb=10: shade the interior, draw no edge.
    const auto result = decoder.process(QByteArray::fromHex(
        "ff93b044b14f404f40b74240"
        "a5427a406244644062436f416792"
        "95"));

    QVERIFY(!result.error);
    const QImage &plane = decoder.graphicsPlane();
    QCOMPARE(plane.pixelColor(239, planeY(57)), QColor(255, 0, 255));
    // The apex is narrow, so a point at the same height but well to its left
    // lies outside the triangle.
    QCOMPARE(plane.pixelColor(190, planeY(100)), QColor(0, 0, 0));
}

void TestGddm5292::boundaryOnlyModeLeavesInteriorClear() {
    Gddm5292Decoder decoder;
    // Fill mode 41/40 is bb=01: solid boundary, no interior.
    const auto result = decoder.process(QByteArray::fromHex(
        "ff93b042b14f404f40b74140"
        "a54079406242674062426741644079416492"
        "95"));

    QVERIFY(!result.error);
    const QImage &plane = decoder.graphicsPlane();
    QCOMPARE(plane.pixelColor(57, planeY(67)), QColor(0, 255, 0));
    QCOMPARE(plane.pixelColor(112, planeY(67)), QColor(0, 0, 0));
}

void TestGddm5292::appliesStyleToCapturedStarFill() {
    Gddm5292Decoder decoder;
    // Colour 6, style 1/3/1/3 (period 8, visible at offsets 0 and 4), fill
    // mode 42/40, then the ten-point star.
    const auto result = decoder.process(QByteArray::fromHex(
        "ff93b046b141434143b74240"
        "a5436f437c4447434e4558434e445a427144724244436f4260426c4244"
        "434442714246434e4357434e92"
        "95"));

    QVERIFY(!result.error);
    const QImage &plane = decoder.graphicsPlane();
    // This outline is traced tip, inner, tip, inner, so it is a simple concave
    // star and its core counts as inside. The vertical style makes the interior
    // a hatch: columns where x % 8 is 0 or 4 are painted, the rest left alone.
    QCOMPARE(plane.pixelColor(240, planeY(197)), QColor(0, 255, 255));
    QCOMPARE(plane.pixelColor(244, planeY(197)), QColor(0, 255, 255));
    QCOMPARE(plane.pixelColor(241, planeY(197)), QColor(0, 0, 0));
    QCOMPARE(plane.pixelColor(243, planeY(197)), QColor(0, 0, 0));
    // Between two arms of the star is outside, whatever the style says.
    QCOMPARE(plane.pixelColor(140, planeY(240)), QColor(0, 0, 0));
}

void TestGddm5292::resumesFillPolygonAcrossBlocks() {
    Gddm5292Decoder decoder;
    // Split mid-coordinate with More Data to Come, the way IBM i splits long
    // orders when a block fills up.
    const auto first = decoder.process(QByteArray::fromHex(
        "ff93b042b14f404f40b74040a540794062426740" "91"));
    QVERIFY(first.handled);
    QVERIFY(!first.error);
    QVERIFY(decoder.graphicsMode());

    const auto second = decoder.process(QByteArray::fromHex(
        "62426741644079416492" "95"));
    QVERIFY(!second.error);
    QVERIFY(second.changed);
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(112, planeY(67)), QColor(0, 255, 0));
}

void TestGddm5292::rejectsFillPolygonOverEdgeLimit() {
    Gddm5292Decoder decoder;
    QByteArray block = QByteArray::fromHex("ff93b042b14f404f40b74040a5");
    // A zigzag whose every edge, the closing one included, is nonhorizontal.
    const int vertices = Gddm5292Decoder::kMaxFillEdges + 2;
    for (int index = 0; index < vertices; ++index)
        block += point(index, 10 + (index % 2));
    block += QByteArray::fromHex("9295");

    const auto result = decoder.process(block);
    QVERIFY(result.handled);
    QVERIFY(result.error);
    QVERIFY(result.errorMessage.contains("nonhorizontal edges"));
    // G4 is nonrecoverable, so the block and graphics mode both end.
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::FatalError);
    QVERIFY(!decoder.graphicsMode());
}

QTEST_MAIN(TestGddm5292)
#include "test_gddm_5292.moc"
