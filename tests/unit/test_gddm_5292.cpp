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

namespace {

QByteArray coordinate(int x, int y) {
    QByteArray data;
    data.append(static_cast<char>(0x40 | ((x >> 6) & 0x0F)));
    data.append(static_cast<char>(0x40 | (x & 0x3F)));
    data.append(static_cast<char>(0x40 | ((y >> 6) & 0x0F)));
    data.append(static_cast<char>(0x40 | (y & 0x3F)));
    return data;
}

QByteArray variableOrder(uint8_t order, const QVector<QPoint> &points) {
    QByteArray data(1, static_cast<char>(order));
    for (const QPoint &point : points)
        data.append(coordinate(point.x(), point.y()));
    data.append(static_cast<char>(0x92));
    return data;
}

} // namespace

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
    void appliesLineStyleAndOffset();
    void drawsScanlinePelPatterns();
    void drawsSelectedMarkerAndRecoversFromG5();
    void appliesFillModeReferenceShift();
    void appliesNestedShieldAreasOnce();
    void rejectsTooManyPolygonEdges();
};

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

void TestGddm5292::appliesLineStyleAndOffset() {
    Gddm5292Decoder decoder;
    QByteArray block = QByteArray::fromHex("ff93b142424040b260");
    block.append(variableOrder(0xA0, {{0, 0}, {10, 0}}));
    block.append(static_cast<char>(0x95));

    const auto result = decoder.process(block);

    QVERIFY(!result.error);
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(1, 287), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(2, 287), QColor(0, 0, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(3, 287), QColor(0, 0, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(4, 287), QColor(255, 255, 255));

    Gddm5292Decoder offsetDecoder;
    QByteArray offsetBlock = QByteArray::fromHex("ff93b144424242b261");
    offsetBlock.append(variableOrder(0xA0, {{0, 0}, {8, 0}}));
    offsetBlock.append(static_cast<char>(0x95));
    offsetDecoder.process(offsetBlock);
    QCOMPARE(offsetDecoder.graphicsPlane().pixelColor(0, 287), QColor(255, 255, 255));
    QCOMPARE(offsetDecoder.graphicsPlane().pixelColor(1, 287), QColor(0, 0, 0));
    QCOMPARE(offsetDecoder.graphicsPlane().pixelColor(2, 287), QColor(0, 0, 0));
    QCOMPARE(offsetDecoder.graphicsPlane().pixelColor(3, 287), QColor(255, 255, 255));

    Gddm5292Decoder backgroundDecoder;
    backgroundDecoder.process(QByteArray::fromHex("ff93b141414040a34795"));
    QCOMPARE(backgroundDecoder.graphicsPlane().pixelColor(0, 100), QColor(255, 255, 255));
    QCOMPARE(backgroundDecoder.graphicsPlane().pixelColor(1, 100), QColor(0, 0, 0));
    QCOMPARE(backgroundDecoder.graphicsPlane().pixelColor(2, 100), QColor(255, 255, 255));
}

void TestGddm5292::drawsScanlinePelPatterns() {
    Gddm5292Decoder decoder;
    QByteArray block = QByteArray::fromHex("ff93b041a1");
    block.append(coordinate(10, 20));
    block.append(QByteArray::fromHex("7c479295"));

    const auto result = decoder.process(block);

    QVERIFY(!result.error);
    for (int x = 10; x <= 13; ++x)
        QCOMPARE(decoder.graphicsPlane().pixelColor(x, 267), QColor(255, 0, 0));
    for (int x = 14; x <= 18; ++x)
        QCOMPARE(decoder.graphicsPlane().pixelColor(x, 267), QColor(0, 0, 0));
    for (int x = 19; x <= 21; ++x)
        QCOMPARE(decoder.graphicsPlane().pixelColor(x, 267), QColor(255, 0, 0));
}

void TestGddm5292::drawsSelectedMarkerAndRecoversFromG5() {
    Gddm5292Decoder decoder;
    QByteArray block = QByteArray::fromHex("ff93b543");
    block.append(variableOrder(0xA4, {{0, 0}, {10, 10}}));
    block.append(static_cast<char>(0x95));

    const auto result = decoder.process(block);

    QVERIFY(result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::RecoverableError);
    QCOMPARE(decoder.statusBytes().left(2), QByteArray::fromHex("c7f5"));
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(10, 277), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(10, 275), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(8, 277), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(8, 275), QColor(0, 0, 0));
}

void TestGddm5292::appliesFillModeReferenceShift() {
    Gddm5292Decoder decoder;
    QByteArray block = QByteArray::fromHex("ff93b141414040b74241");
    block.append(variableOrder(0xA5, {{10, 10}, {30, 10}, {30, 30}, {10, 30}}));
    block.append(static_cast<char>(0x95));

    const auto result = decoder.process(block);

    QVERIFY(!result.error);
    QCOMPARE(decoder.graphicsPlane().pixelColor(12, 267), QColor(0, 0, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(13, 267), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(13, 277), QColor(0, 0, 0));
}

void TestGddm5292::appliesNestedShieldAreasOnce() {
    Gddm5292Decoder decoder;
    const QVector<QPoint> fill{{10, 10}, {30, 10}, {30, 30}, {10, 30}};
    QByteArray first = QByteArray::fromHex("ff93");
    first.append(variableOrder(0xA6, {{14, 14}, {26, 14}, {26, 26}, {14, 26}}));
    first.append(variableOrder(0xA6, {{18, 18}, {22, 18}, {22, 22}, {18, 22}}));
    first.append(variableOrder(0xA5, fill));

    const auto shielded = decoder.process(first);

    QVERIFY(!shielded.error);
    QCOMPARE(decoder.graphicsPlane().pixelColor(12, 267), QColor(255, 255, 255));
    QCOMPARE(decoder.graphicsPlane().pixelColor(16, 267), QColor(0, 0, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(20, 267), QColor(255, 255, 255));

    QByteArray second = variableOrder(0xA5, fill);
    second.append(static_cast<char>(0x95));
    decoder.process(second);
    QCOMPARE(decoder.graphicsPlane().pixelColor(16, 267), QColor(255, 255, 255));
}

void TestGddm5292::rejectsTooManyPolygonEdges() {
    Gddm5292Decoder decoder;
    QVector<QPoint> points;
    for (int x = 0; x < 130; ++x)
        points.append(QPoint(x, 10 + (x & 1)));
    QByteArray block(1, static_cast<char>(0xFF));
    block.append(variableOrder(0xA5, points));

    const auto result = decoder.process(block);

    QVERIFY(result.error);
    QCOMPARE(result.completion, Gddm5292Decoder::Completion::FatalError);
    QCOMPARE(decoder.statusBytes().left(2), QByteArray::fromHex("c7f4"));
    QVERIFY(!decoder.graphicsMode());
}

QTEST_MAIN(TestGddm5292)
#include "test_gddm_5292.moc"
