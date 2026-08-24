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
    void decodesCapturedGddmDemoBlock();
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
    QVERIFY(result.pacingResponse);
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
    QVERIFY(first.pacingResponse);
    QVERIFY(decoder.graphicsMode());
    QVERIFY(decoder.graphicsPlane().pixelColor(0, 277).alpha() == 0);

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
    QVERIFY(reset.pacingResponse);
    QVERIFY(!decoder.displayEnabled());
    QVERIFY(!decoder.graphicsMode());
    QVERIFY(decoder.graphicsPlane().pixelColor(0, 277).alpha() == 0);
}

void TestGddm5292::suppressesPacingWhenRequested() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ff9695"));
    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QVERIFY(!result.pacingResponse);
}

void TestGddm5292::rejectsMalformedCoordinates() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ffa04040404a92"));
    QVERIFY(result.handled);
    QVERIFY(result.error);
    QVERIFY(!result.pacingResponse);
    QVERIFY(!decoder.graphicsMode());
}

void TestGddm5292::decodesReadStatusOffset() {
    Gddm5292Decoder decoder;
    const auto result = decoder.process(QByteArray::fromHex("ff804043959090909090"));
    QVERIFY(result.handled);
    QVERIFY(!result.error);
    QVERIFY(result.pacingResponse);
    QCOMPARE(result.readStatusOffset, 3);
    QVERIFY(!decoder.graphicsMode());
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
    QVERIFY(result.pacingResponse);
    QVERIFY(result.changed);
    QVERIFY(decoder.displayEnabled());
    QVERIFY(!decoder.graphicsMode());
    QCOMPARE(decoder.graphicsPlane().pixelColor(0, 287), QColor(0, 255, 0));
    QCOMPARE(decoder.graphicsPlane().pixelColor(431, 29), QColor(0, 255, 0));
}

QTEST_MAIN(TestGddm5292)
#include "test_gddm_5292.moc"
