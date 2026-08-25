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
    void drawsCapturedImageScanlines();
    void scanlinePatternIsSixBitsMsbFirst();
    void drawsCapturedPolymarkers();
    void rejectsMarkerAboveEight();
    void markerOutsideDisplayIsRecoverable();
    void styleOffsetSelectsStartingSegment();
    void warnsOnBlockBelowMinimumLength();
    void warnsOnBlockBeyondDeviceLimit();
    void reportsSuppressedPacingDistinctly();
    void retainsLastOrderAndRawBlock();
    void decodesCapturedPrinterGraphicsMixTable();
    void rejectsFixedAlphanumericMixIndexAsP5();
    void loadsChangeableAlphanumericMixIndexes();
    void retainsPrinterDataAndTimeout();
    void reportsScreenCopyRequest();
    void resumesOrderSplitBeforeAnyDataArrives();
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

// The primitive fixtures below are the exact bytes IBM i sent for a probe that
// draws markers, text in both character modes and a GSIMG image, taken from
// GDDM/captures/gddm-primitives.pcap.

void TestGddm5292::drawsCapturedImageScanlines() {
    Gddm5292Decoder decoder;
    // GSIMG of an 8x8 image whose first row is EBCDIC 'A' (0xC1) and whose
    // remaining rows are blanks (0x40). IBM i lowers it to eight A1 orders at
    // device x=47, rows y=57 down to y=50: top row first, decreasing Y.
    const auto result = decoder.process(QByteArray::fromHex("ff93b047"
        "a1406f4079705092" "a1406f4078504092" "a1406f4077504092"
        "a1406f4076504092" "a1406f4075504092" "a1406f4074504092"
        "a1406f4073504092" "a1406f4072504092" "95"));

    QVERIFY(!result.error);
    QVERIFY(result.changed);

    const QImage &plane = decoder.graphicsPlane();
    // Row 0 is 11000001: PELs at offsets 0, 1 and 7.
    const int top = planeY(57);
    QCOMPARE(plane.pixelColor(47, top), QColor(255, 255, 255));
    QCOMPARE(plane.pixelColor(48, top), QColor(255, 255, 255));
    QCOMPARE(plane.pixelColor(49, top), QColor(0, 0, 0));
    QCOMPARE(plane.pixelColor(54, top), QColor(255, 255, 255));
    // The remaining rows are 01000000: one PEL at offset 1 only.
    for (int row = 1; row < 8; ++row) {
        const int y = planeY(57 - row);
        QCOMPARE(plane.pixelColor(47, y), QColor(0, 0, 0));
        QCOMPARE(plane.pixelColor(48, y), QColor(255, 255, 255));
    }
}

void TestGddm5292::scanlinePatternIsSixBitsMsbFirst() {
    Gddm5292Decoder decoder;
    // One data byte carries six PELs, most significant bit leftmost, so 0x2A
    // (101010) paints alternating PELs from the start coordinate.
    const auto result = decoder.process(
        QByteArray::fromHex("ff93b047" "a1404a404a" "6a" "92" "95"));

    QVERIFY(!result.error);
    const QImage &plane = decoder.graphicsPlane();
    const int y = planeY(10);
    QCOMPARE(plane.pixelColor(10, y), QColor(255, 255, 255));
    QCOMPARE(plane.pixelColor(11, y), QColor(0, 0, 0));
    QCOMPARE(plane.pixelColor(12, y), QColor(255, 255, 255));
    QCOMPARE(plane.pixelColor(14, y), QColor(255, 255, 255));
    // Nothing beyond the six PELs the byte describes.
    QCOMPARE(plane.pixelColor(16, y), QColor(0, 0, 0));
}

void TestGddm5292::drawsCapturedPolymarkers() {
    Gddm5292Decoder decoder;
    // Set Marker 4 then Write Polymarker at device (95,229), as captured.
    // Marker 4 is the 5x5 cross.
    const auto result = decoder.process(
        QByteArray::fromHex("ff93" "b544b047b343" "a4415f436592" "95"));

    QVERIFY(!result.error);
    const QImage &plane = decoder.graphicsPlane();
    const int cy = planeY(229);
    QCOMPARE(plane.pixelColor(93, cy - 2), QColor(255, 255, 255)); // corner
    QCOMPARE(plane.pixelColor(95, cy), QColor(255, 255, 255));     // centre
    QCOMPARE(plane.pixelColor(97, cy + 2), QColor(255, 255, 255)); // corner
    QCOMPARE(plane.pixelColor(94, cy), QColor(0, 0, 0));           // arm gap
    QCOMPARE(plane.pixelColor(95, cy - 2), QColor(0, 0, 0));
}

void TestGddm5292::rejectsMarkerAboveEight() {
    Gddm5292Decoder decoder;
    // Only nine shapes exist; the manual lists "Set marker > 8" as G3.
    const auto result = decoder.process(QByteArray::fromHex("ff93b54995"));
    QVERIFY(result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::FatalError);
    QVERIFY(!decoder.graphicsMode());
}

void TestGddm5292::markerOutsideDisplayIsRecoverable() {
    Gddm5292Decoder decoder;
    // Two markers: (1,1) cannot fit its 5x5 box, (95,229) can. G5 is the one
    // recoverable graphics error, so the device skips the bad one, finishes the
    // block, keeps graphics mode and answers Cmd-9 rather than Cmd-10.
    const auto result = decoder.process(QByteArray::fromHex(
        "ff93" "b544b047" "a4" "40414041" "415f4365" "92" "90"));

    QVERIFY(result.handled);
    QVERIFY(result.error);
    QVERIFY(result.errorMessage.startsWith("G5 at offset"));
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::RecoverableError);
    // Recoverable means graphics mode survives, and 0x90 keeps it open.
    QVERIFY(decoder.graphicsMode());
    // The in-bounds marker was still drawn.
    QCOMPARE(decoder.graphicsPlane().pixelColor(95, planeY(229)),
             QColor(255, 255, 255));
}

void TestGddm5292::styleOffsetSelectsStartingSegment() {
    // A square spanning device x 8..30, y 10..30, hatched by style 1/3/1/3
    // (period 8, visible at phases 0 and 4). The fill phase is the X
    // coordinate, so column 8 falls on phase 0 and is painted.
    const QByteArray prefix = QByteArray::fromHex("ff93b1" "41434143" "b047");
    const QByteArray fill = QByteArray::fromHex("b74240a5")
        + QByteArray::fromHex("4048404a405e404a405e405e4048405e")
        + QByteArray::fromHex("9295");

    Gddm5292Decoder unshifted;
    QVERIFY(!unshifted.process(prefix + fill).error);

    // Set Style Offset 0x50 selects segment 1, the first gap, so the same
    // column now lands in a gap instead.
    Gddm5292Decoder shifted;
    QVERIFY(!shifted.process(prefix + QByteArray::fromHex("b250") + fill).error);

    const int y = planeY(20);
    QCOMPARE(unshifted.graphicsPlane().pixelColor(8, y), QColor(255, 255, 255));
    QCOMPARE(shifted.graphicsPlane().pixelColor(8, y), QColor(0, 0, 0));
}

// Diagnostics report; they never reject a block.

void TestGddm5292::warnsOnBlockBelowMinimumLength() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ff9395"));
    QVERIFY(result.handled);
    QVERIFY(!result.error);   // short, but perfectly well formed
    QVERIFY(result.warning.contains("below the documented"));
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::Success);
}

void TestGddm5292::warnsOnBlockBeyondDeviceLimit() {
    Gddm5292Decoder decoder;
    // Valid throughout: Graphics Display On repeated past the 256-byte limit.
    QByteArray block = QByteArray::fromHex("ff");
    block += QByteArray(300, static_cast<char>(0x93));
    block += QByteArray::fromHex("95");

    const auto result = decoder.process(block);
    QVERIFY(!result.error);   // never rejected on length alone
    QVERIFY(result.warning.contains("device limit"));
}

void TestGddm5292::reportsSuppressedPacingDistinctly() {
    Gddm5292Decoder decoder;
    const auto suppressed = decoder.process(QByteArray::fromHex("ff9396b04195"));
    QCOMPARE(suppressed.completion, Gddm5292Decoder::Completion::None);
    QVERIFY(suppressed.pacingSuppressed);   // deliberately silent

    Gddm5292Decoder other;
    const auto normal = other.process(QByteArray::fromHex("ff93b04195"));
    QCOMPARE(normal.completion, Gddm5292Decoder::Completion::Success);
    QVERIFY(!normal.pacingSuppressed);
}

void TestGddm5292::retainsLastOrderAndRawBlock() {
    Gddm5292Decoder decoder;
    const QByteArray block = QByteArray::fromHex("ff93b041a04040404a416440729295");
    decoder.process(block);
    // A0 was the last set or draw order; the trailing 92 and 95 are control
    // bytes and do not displace it.
    QCOMPARE(decoder.lastOrder(), uint8_t(0xA0));
    QCOMPARE(decoder.lastBlock(), block);
}

// The printer orders are retained state, not behaviour: there is no attached
// printer, and they describe how one would render the picture.

void TestGddm5292::decodesCapturedPrinterGraphicsMixTable() {
    Gddm5292Decoder decoder;
    // The exact C3 order IBM i sends in every picture's opening block: seven
    // index/bcmy pairs for colour indexes 1..7, mapping each display colour to
    // its subtractive printer equivalent, white becoming black ink on paper.
    const auto result = decoder.process(QByteArray::fromHex(
        "ff93" "c3" "4146424343424445454446414748" "92" "95"));
    QVERIFY(result.handled);
    QVERIFY(!result.error);

    const auto &mix = decoder.printerGraphicsMix();
    QCOMPARE(mix[1], uint8_t(0x6)); // blue    -> cyan+magenta
    QCOMPARE(mix[2], uint8_t(0x3)); // red     -> magenta+yellow
    QCOMPARE(mix[3], uint8_t(0x2)); // magenta -> magenta
    QCOMPARE(mix[4], uint8_t(0x5)); // green   -> cyan+yellow
    QCOMPARE(mix[5], uint8_t(0x4)); // cyan    -> cyan
    QCOMPARE(mix[6], uint8_t(0x1)); // yellow  -> yellow
    QCOMPARE(mix[7], uint8_t(0x8)); // white   -> black
    QCOMPARE(mix[0], uint8_t(0x0)); // index 0 keeps its default of no ink
}

void TestGddm5292::rejectsFixedAlphanumericMixIndexAsP5() {
    Gddm5292Decoder decoder;
    // Index 7 is fixed in the device. The manual names this exact case as P5,
    // which is nonrecoverable, so the block ends with Cmd-10.
    const auto result = decoder.process(QByteArray::fromHex("ff93c247409295"));
    QVERIFY(result.error);
    QVERIFY(result.errorMessage.startsWith("P5 at offset"));
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::FatalError);
    QVERIFY(!decoder.graphicsMode());
    // The status bytes carry the code as EBCDIC 'P' then '5'.
    const QByteArray status = decoder.statusBytes();
    QCOMPARE(static_cast<uint8_t>(status[0]), uint8_t(0xD7));
    QCOMPARE(static_cast<uint8_t>(status[1]), uint8_t(0xF5));
}

void TestGddm5292::loadsChangeableAlphanumericMixIndexes() {
    Gddm5292Decoder decoder;
    // Indexes 2 and 8 are changeable; 0x4F carries bcmy 1111.
    const auto result = decoder.process(
        QByteArray::fromHex("ff93c2" "424f" "484c" "9295"));
    QVERIFY(!result.error);
    QCOMPARE(decoder.printerAlphaMix()[2], uint8_t(0xF));
    QCOMPARE(decoder.printerAlphaMix()[8], uint8_t(0xC));
    // A fixed index keeps its default of no ink.
    QCOMPARE(decoder.printerAlphaMix()[7], uint8_t(0x0));
}

void TestGddm5292::retainsPrinterDataAndTimeout() {
    Gddm5292Decoder decoder;
    QCOMPARE(decoder.printerTimeoutUnits(), 3);   // default is three units

    const auto result = decoder.process(
        QByteArray::fromHex("ff93" "c444" "c04142434492" "95"));
    QVERIFY(!result.error);
    QCOMPARE(decoder.printerTimeoutUnits(), 4);   // 4 x 5.5 = 22 seconds
    QCOMPARE(decoder.printerData(), QByteArray::fromHex("41424344"));
}

void TestGddm5292::reportsScreenCopyRequest() {
    Gddm5292Decoder decoder;
    const auto quiet = decoder.process(QByteArray::fromHex("ff93b04195"));
    QVERIFY(!quiet.screenCopyRequested);

    Gddm5292Decoder other;
    const auto copy = other.process(QByteArray::fromHex("ff93c195"));
    QVERIFY(copy.handled);
    QVERIFY(!copy.error);
    QVERIFY(copy.screenCopyRequested);
    // The order is acknowledged like any other block.
    QCOMPARE(copy.completion, Gddm5292Decoder::Completion::Success);
}

void TestGddm5292::resumesOrderSplitBeforeAnyDataArrives() {
    Gddm5292Decoder decoder;
    // IBM i splits a block immediately after an order byte, leaving all of that
    // order's coordinate data for the next block, so More Data to Come can
    // arrive with nothing buffered. Taken from a real GSAREA capture, where a
    // polyline's A0 and its 91 sat at the end of one block.
    const auto first = decoder.process(QByteArray::fromHex("ff93b041a091"));
    QVERIFY(first.handled);
    QVERIFY(!first.error);
    QVERIFY(decoder.graphicsMode());

    const auto second = decoder.process(
        QByteArray::fromHex("404a404a4054404a" "92" "95"));
    QVERIFY(!second.error);
    QVERIFY(second.changed);
    QCOMPARE(decoder.graphicsPlane().pixelColor(12, planeY(10)), QColor(255, 0, 0));
}

QTEST_MAIN(TestGddm5292)
#include "test_gddm_5292.moc"
