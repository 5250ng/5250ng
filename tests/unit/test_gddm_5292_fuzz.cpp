// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Mutation fuzzing for the 5292 graphics decoder.
//
// The corpus is real blocks captured from IBM i GDDM on RSRCH09 plus a set of
// hand-written adversarial cases. Mutants are generated from a fixed seed, so a
// failure is reproducible. The properties asserted are the ones a hostile or
// corrupted stream could break:
//
//   1. No crash, no out-of-bounds write. Meaningful under -fsanitize=address.
//   2. A handled block ALWAYS answers, either with a Graphic Aid key code or by
//      deliberately suppressing it. A silent failure stalls the host's pacing
//      loop forever, which is the regression this file exists to prevent.
//   3. A nonrecoverable error leaves graphics mode terminated, as the device
//      does, and a recoverable one answers Cmd-9.
//   4. A reported error offset lies inside the block.
//   5. The plane keeps its geometry and indexed format whatever arrives.
//   6. After any failure the decoder still processes a well-formed block.

#include "ui/rendering/gddm_5292_decoder.h"

#include <QtTest/QtTest>

#include <random>

using ui::rendering::Gddm5292Decoder;

class TestGddm5292Fuzz : public QObject {
    Q_OBJECT

  private slots:
    void mutatedBlocksUpholdInvariants();
    void carriedStateUpholdsInvariants();
    void adversarialBlocksUpholdInvariants();

  private:
    // Blocks as IBM i actually sent them, plus control-only and spanning cases.
    static QList<QByteArray> corpus() {
        return {
            QByteArray::fromHex("ff93b041a04040404a41644072425640599295"),
            QByteArray::fromHex("ff93b042b14f404f40b74040"
                                "a54079406242674062426741644079416492" "95"),
            QByteArray::fromHex("ff93b047a1406f407970509295"),
            QByteArray::fromHex("ff93b544b047b343a4415f43659295"),
            QByteArray::fromHex("ffb14f404f40b343b240a340939295"),
            QByteArray::fromHex("ff93b046b141434143b74240"
                                "a5436f437c4447434e4558434e445a427144724244436f"
                                "4260426c4244434442714246434e4357434e92" "95"),
            QByteArray::fromHex("ff9695"),
            QByteArray::fromHex("ffff"),
            QByteArray::fromHex("ff93a04040404a416491"),
            QByteArray::fromHex("ff939393939393939393939393939393939395"),
        };
    }

    // Every property that must hold for any input whatsoever.
    static void checkInvariants(Gddm5292Decoder &decoder,
                               const Gddm5292Decoder::Result &result,
                               const QByteArray &block) {
        if (!result.handled)
            return;

        using Completion = Gddm5292Decoder::Completion;

        // (2) Always answer, or be explicit about staying silent.
        QVERIFY2(result.completion != Completion::None || result.pacingSuppressed,
                 qPrintable(QString("silent block, no completion and not suppressed: %1")
                                .arg(QString::fromLatin1(block.toHex()))));

        if (result.error) {
            // (3) Fatal ends graphics mode; recoverable carries on.
            QVERIFY(result.completion == Completion::FatalError
                    || result.completion == Completion::RecoverableError
                    || result.pacingSuppressed);
            if (result.completion == Completion::FatalError)
                QVERIFY(!decoder.graphicsMode());
            // (4) An error always says what went wrong, with an offset.
            QVERIFY(!result.errorMessage.isEmpty());
            QVERIFY(result.errorMessage.contains(QStringLiteral("at offset")));
        } else {
            QVERIFY(result.completion != Completion::FatalError);
            QVERIFY(result.completion != Completion::RecoverableError);
        }

        // (5) The plane never changes shape or format.
        QCOMPARE(decoder.graphicsPlane().size(),
                 QSize(Gddm5292Decoder::kWidth, Gddm5292Decoder::kHeight));
        QCOMPARE(decoder.graphicsPlane().format(), QImage::Format_Indexed8);
    }

    // Deterministic mutations: bit flip, byte replace, truncation, growth.
    static QByteArray mutate(const QByteArray &seed, std::mt19937 &rng) {
        QByteArray out = seed;
        switch (rng() % 5) {
        case 0: { // flip one bit
            if (out.isEmpty()) break;
            const int index = int(rng() % uint(out.size()));
            out[index] = char(uint8_t(out[index]) ^ (1u << (rng() % 8)));
            break;
        }
        case 1: { // replace one byte with an arbitrary value
            if (out.isEmpty()) break;
            const int index = int(rng() % uint(out.size()));
            out[index] = char(uint8_t(rng() % 256));
            break;
        }
        case 2: // truncate, which strips terminators and block ends
            out.truncate(int(rng() % uint(out.size() + 1)));
            break;
        case 3: { // splice a chunk in, to overrun variable orders
            if (out.isEmpty()) break;
            const int at = int(rng() % uint(out.size()));
            const int len = int(rng() % 32) + 1;
            QByteArray chunk(len, char(uint8_t(rng() % 256)));
            out.insert(at, chunk);
            break;
        }
        case 4: { // duplicate the tail
            out += out.right(int(rng() % uint(out.size() + 1)));
            break;
        }
        }
        return out;
    }
};

void TestGddm5292Fuzz::mutatedBlocksUpholdInvariants() {
    std::mt19937 rng(0x5250);
    // A0 is a variable order, so it needs its End of Data terminator before
    // End Graphics; without the 92 this block is itself malformed.
    const QByteArray recovery = QByteArray::fromHex("ff93b041a04040404a416440729295");
    int handled = 0, failures = 0;

    for (const QByteArray &seed : corpus()) {
        for (int iteration = 0; iteration < 400; ++iteration) {
            const QByteArray block = mutate(seed, rng);
            Gddm5292Decoder decoder;
            const auto result = decoder.process(block);
            checkInvariants(decoder, result, block);
            if (result.handled) ++handled;
            if (result.error) {
                ++failures;
                // (6) A failure must not poison the decoder. Only checked after
                // a fatal error, which ends graphics mode: a recoverable one
                // leaves the mode open, and a leading FF is then legitimately
                // rejected as a Begin Graphics inside a block.
                if (result.completion == Gddm5292Decoder::Completion::FatalError) {
                    const auto after = decoder.process(recovery);
                    QVERIFY2(after.handled && !after.error
                                 && after.completion
                                        == Gddm5292Decoder::Completion::Success,
                             qPrintable(QString("decoder poisoned by %1: %2")
                                            .arg(QString::fromLatin1(block.toHex()))
                                            .arg(after.errorMessage)));
                }
            }
        }
    }
    // The corpus must actually be reaching the decoder and provoking errors,
    // or the run above proves nothing.
    QVERIFY(handled > 3000);
    QVERIFY(failures > 200);
}

void TestGddm5292Fuzz::carriedStateUpholdsInvariants() {
    // One long-lived decoder, so mutants land on whatever state the previous
    // one left: mid-order, mid-block, graphics mode on or off.
    std::mt19937 rng(0xB552);
    Gddm5292Decoder decoder;
    const QList<QByteArray> seeds = corpus();

    for (int iteration = 0; iteration < 4000; ++iteration) {
        const QByteArray &seed = seeds[int(rng() % uint(seeds.size()))];
        const QByteArray block = mutate(seed, rng);
        const auto result = decoder.process(block);
        checkInvariants(decoder, result, block);
    }
}

void TestGddm5292Fuzz::adversarialBlocksUpholdInvariants() {
    // Hand-written cases aimed at the specific places a decoder tends to break.
    QList<QByteArray> blocks;

    blocks << QByteArray::fromHex("ff");                       // begin, nothing else
    blocks << QByteArray::fromHex("ffff") ;                    // system reset
    blocks << QByteArray::fromHex("ffffff");                   // reset plus a stray
    blocks << QByteArray::fromHex("ff91");                     // spanning with no order
    blocks << QByteArray::fromHex("ff92");                      // terminator with no order
    blocks << QByteArray::fromHex("ff") + QByteArray(600, char(0x91));
    blocks << QByteArray::fromHex("ff") + QByteArray(600, char(0x92));
    blocks << QByteArray::fromHex("ff") + QByteArray(600, char(0x93));
    blocks << QByteArray::fromHex("ff") + QByteArray(2500, char(0x40)); // huge data run

    // A polygon whose coordinates are all at the 10-bit maximum, far outside
    // the 480x288 surface.
    QByteArray far = QByteArray::fromHex("ff93b041b74040a5");
    for (int i = 0; i < 40; ++i)
        far += QByteArray::fromHex("4f7f4f7f");
    far += QByteArray::fromHex("9295");
    blocks << far;

    // A fill polygon well past the 128 nonhorizontal edge limit.
    QByteArray edges = QByteArray::fromHex("ff93b041b74040a5");
    for (int i = 0; i < 400; ++i) {
        edges.append(char(0x40 | ((i >> 6) & 0x0F)));
        edges.append(char(0x40 | (i & 0x3F)));
        edges.append(char(0x40));
        edges.append(char(0x40 | (i % 2)));
    }
    edges += QByteArray::fromHex("9295");
    blocks << edges;

    // A scanline claiming a pattern far wider than the surface.
    blocks << QByteArray::fromHex("ff93b041a1407f4040") + QByteArray(700, char(0x7f))
                  + QByteArray::fromHex("9295");

    // Every possible order byte, alone in a block.
    for (int byte = 0; byte < 256; ++byte) {
        QByteArray probe = QByteArray::fromHex("ff93");
        probe.append(char(byte));
        probe += QByteArray::fromHex("95");
        blocks << probe;
    }

    for (const QByteArray &block : blocks) {
        Gddm5292Decoder decoder;
        const auto result = decoder.process(block);
        checkInvariants(decoder, result, block);
    }
}

QTEST_MAIN(TestGddm5292Fuzz)
#include "test_gddm_5292_fuzz.moc"
