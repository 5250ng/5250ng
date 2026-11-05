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

#include "core/codepage.h"
#include <QtTest/QtTest>

using namespace core;

class TestCodePage : public QObject {
    Q_OBJECT

  private slots:
    // Basic functionality
    void testSupportedCodePages();
    void testCodePageNames();
    void testSpaceMapping();

    // CP037 specific tests
    void testCP037Letters();
    void testCP037Digits();
    void testCP037SpecialChars();

    // Roundtrip tests
    void testRoundtripCP037();
    void testRoundtripAllCodePages();

    // Bulk conversion
    void testBulkToUnicode();
    void testBulkFromUnicode();
    void testBulkEmpty();

    // Edge cases
    void testNothingMapsToControlRange();
    void testIbmAnchors_data();
    void testIbmAnchors();
    void testGraphicRangeRoundTrips();
    void testThaiToneMarksEncodeLikeIbm();
    void testNulEncodesToNul();
    void testUnmappableUnicode();
    void testReplacementChar();

    // Cross-codepage letter consistency
    void testLettersConsistentAcrossCodePages();
    void testDigitsConsistentAcrossCodePages();
};

void TestCodePage::testSupportedCodePages() {
    QList<CodePage::ID> pages = CodePage::supportedCodePages();
    QCOMPARE(pages.size(), 13);
    QVERIFY(pages.contains(CodePage::ID::CP037));
    QVERIFY(pages.contains(CodePage::ID::CP273));
    QVERIFY(pages.contains(CodePage::ID::CP500));
    QVERIFY(pages.contains(CodePage::ID::CP870));
    QVERIFY(pages.contains(CodePage::ID::CP420));
    QVERIFY(pages.contains(CodePage::ID::CP424));
    QVERIFY(pages.contains(CodePage::ID::CP838));
}

void TestCodePage::testCodePageNames() {
    QVERIFY(CodePage::codepageName(CodePage::ID::CP037).contains("US"));
    QVERIFY(CodePage::codepageName(CodePage::ID::CP273).contains("Germany"));
    QVERIFY(CodePage::codepageName(CodePage::ID::CP285).contains("UK"));
    QVERIFY(CodePage::codepageName(CodePage::ID::CP297).contains("France"));

    // Name via instance
    CodePage cp(CodePage::ID::CP037);
    QCOMPARE(cp.name(), CodePage::codepageName(CodePage::ID::CP037));
}

void TestCodePage::testSpaceMapping() {
    // EBCDIC 0x40 = space in ALL code pages
    for (auto id : CodePage::supportedCodePages()) {
        CodePage cp(id);
        QCOMPARE(cp.toUnicode(0x40), QChar(' '));
    }
}

void TestCodePage::testCP037Letters() {
    CodePage cp(CodePage::ID::CP037);

    // Lowercase a-i at 0x81-0x89
    QCOMPARE(cp.toUnicode(0x81), QChar('a'));
    QCOMPARE(cp.toUnicode(0x89), QChar('i'));

    // Lowercase j-r at 0x91-0x99
    QCOMPARE(cp.toUnicode(0x91), QChar('j'));
    QCOMPARE(cp.toUnicode(0x99), QChar('r'));

    // Lowercase s-z at 0xA2-0xA9
    QCOMPARE(cp.toUnicode(0xA2), QChar('s'));
    QCOMPARE(cp.toUnicode(0xA9), QChar('z'));

    // Uppercase A-I at 0xC1-0xC9
    QCOMPARE(cp.toUnicode(0xC1), QChar('A'));
    QCOMPARE(cp.toUnicode(0xC9), QChar('I'));

    // Uppercase J-R at 0xD1-0xD9
    QCOMPARE(cp.toUnicode(0xD1), QChar('J'));
    QCOMPARE(cp.toUnicode(0xD9), QChar('R'));

    // Uppercase S-Z at 0xE2-0xE9
    QCOMPARE(cp.toUnicode(0xE2), QChar('S'));
    QCOMPARE(cp.toUnicode(0xE9), QChar('Z'));
}

void TestCodePage::testCP037Digits() {
    CodePage cp(CodePage::ID::CP037);

    // Digits 0-9 at 0xF0-0xF9
    for (int d = 0; d <= 9; ++d) {
        QCOMPARE(cp.toUnicode(0xF0 + d), QChar('0' + d));
    }
}

void TestCodePage::testCP037SpecialChars() {
    CodePage cp(CodePage::ID::CP037);

    QCOMPARE(cp.toUnicode(0x4B), QChar('.')); // period
    QCOMPARE(cp.toUnicode(0x4C), QChar('<')); // less than
    QCOMPARE(cp.toUnicode(0x4D), QChar('(')); // left paren
    QCOMPARE(cp.toUnicode(0x4E), QChar('+')); // plus
    QCOMPARE(cp.toUnicode(0x50), QChar('&')); // ampersand
    QCOMPARE(cp.toUnicode(0x5D), QChar(')')); // right paren
    QCOMPARE(cp.toUnicode(0x5E), QChar(';')); // semicolon
    QCOMPARE(cp.toUnicode(0x60), QChar('-')); // minus
    QCOMPARE(cp.toUnicode(0x61), QChar('/')); // slash
    QCOMPARE(cp.toUnicode(0x6B), QChar(',')); // comma
    QCOMPARE(cp.toUnicode(0x7A), QChar(':')); // colon
    QCOMPARE(cp.toUnicode(0x7D), QChar('\'')); // apostrophe
    QCOMPARE(cp.toUnicode(0x7E), QChar('=')); // equals
    QCOMPARE(cp.toUnicode(0x7F), QChar('"')); // double quote
}

void TestCodePage::testRoundtripCP037() {
    CodePage cp(CodePage::ID::CP037);

    // All printable ASCII should roundtrip
    QString ascii = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (QChar ch : ascii) {
        uint8_t ebcdic = cp.fromUnicode(ch);
        QChar back = cp.toUnicode(ebcdic);
        QCOMPARE(back, ch);
    }

    // Common punctuation
    QString punct = ".,;:!?+-*/=()[]{}@#$%&<>";
    for (QChar ch : punct) {
        uint8_t ebcdic = cp.fromUnicode(ch);
        if (ebcdic == 0x40 && ch != ' ') continue; // unmappable
        QChar back = cp.toUnicode(ebcdic);
        QCOMPARE(back, ch);
    }
}

void TestCodePage::testRoundtripAllCodePages() {
    // Letters and digits should roundtrip in ALL code pages
    QString lettersDigits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    for (auto id : CodePage::supportedCodePages()) {
        CodePage cp(id);
        for (QChar ch : lettersDigits) {
            uint8_t ebcdic = cp.fromUnicode(ch);
            QChar back = cp.toUnicode(ebcdic);
            QVERIFY2(back == ch,
                     qPrintable(QString("Roundtrip failed for '%1' in codepage %2: got '%3'")
                                    .arg(ch).arg(static_cast<int>(id)).arg(back)));
        }
    }
}

void TestCodePage::testBulkToUnicode() {
    CodePage cp(CodePage::ID::CP037);

    // "HELLO" in EBCDIC CP037
    QByteArray ebcdic;
    ebcdic.append(static_cast<char>(0xC8)); // H
    ebcdic.append(static_cast<char>(0xC5)); // E
    ebcdic.append(static_cast<char>(0xD3)); // L
    ebcdic.append(static_cast<char>(0xD3)); // L
    ebcdic.append(static_cast<char>(0xD6)); // O

    QString result = cp.toUnicode(ebcdic);
    QCOMPARE(result, QString("HELLO"));
}

void TestCodePage::testBulkFromUnicode() {
    CodePage cp(CodePage::ID::CP037);

    QByteArray result = cp.fromUnicode("HELLO");
    QCOMPARE(result.size(), 5);
    QCOMPARE(static_cast<uint8_t>(result[0]), static_cast<uint8_t>(0xC8)); // H
    QCOMPARE(static_cast<uint8_t>(result[1]), static_cast<uint8_t>(0xC5)); // E
    QCOMPARE(static_cast<uint8_t>(result[4]), static_cast<uint8_t>(0xD6)); // O
}

void TestCodePage::testBulkEmpty() {
    CodePage cp(CodePage::ID::CP037);

    QCOMPARE(cp.toUnicode(QByteArray()), QString());
    QCOMPARE(cp.fromUnicode(QString()), QByteArray());
}

void TestCodePage::testNothingMapsToControlRange() {
    // Issue #267: no Unicode character other than NUL may encode to a 5250
    // order or control byte (0x01-0x3F), in any code page. The reported
    // case: '#' and '$' on CP273 were sent as 0x23 and 0x24
    CodePage cp273(CodePage::ID::CP273);
    QVERIFY(cp273.fromUnicode(QChar('#')) >= 0x40);
    QVERIFY(cp273.fromUnicode(QChar('$')) >= 0x40);
    for (auto id : CodePage::supportedCodePages()) {
        CodePage cp(id);
        for (int u = 1; u < 0x10000; ++u) {
            const uint8_t b = cp.fromUnicode(QChar(static_cast<char16_t>(u)));
            if (b < 0x40) {
                QFAIL(qPrintable(QString("code page %1 encodes U+%2 as control byte 0x%3")
                                     .arg(static_cast<int>(id))
                                     .arg(u, 4, 16, QChar('0'))
                                     .arg(b, 2, 16, QChar('0'))));
            }
        }
    }
    CodePage cp(CodePage::ID::CP037);
    QCOMPARE(cp.fromUnicode(QChar(0x0000)), static_cast<uint8_t>(0x00));
    QCOMPARE(cp.fromUnicode(QChar(0x0001)), static_cast<uint8_t>(0x40));
}

// Issue #266: bytes the hand-written tables had wrong. Expected values come
// from Python's codecs where it has the code page and from IBM's Java
// charsets (IBM2xx, IBM-Thai) otherwise, not from ICU, which the tables are
// generated from (tools/gen_codepage_tables.py)
void TestCodePage::testIbmAnchors_data() {
    QTest::addColumn<CodePage::ID>("id");
    QTest::addColumn<int>("byte");
    QTest::addColumn<int>("unicode");
    QTest::newRow("CP037 0x5F") << CodePage::ID::CP037 << 0x5F << 0x00AC; // Python cp037
    QTest::newRow("CP037 0xAD") << CodePage::ID::CP037 << 0xAD << 0x00DD; // Python cp037
    QTest::newRow("CP037 0xBA") << CodePage::ID::CP037 << 0xBA << 0x005B; // Python cp037
    QTest::newRow("CP037 0xBD") << CodePage::ID::CP037 << 0xBD << 0x00A8; // Python cp037
    QTest::newRow("CP037 0xBB") << CodePage::ID::CP037 << 0xBB << 0x005D; // Python cp037
    QTest::newRow("CP273 0x43") << CodePage::ID::CP273 << 0x43 << 0x007B; // Python cp273
    QTest::newRow("CP273 0x59") << CodePage::ID::CP273 << 0x59 << 0x007E; // Python cp273
    QTest::newRow("CP273 0xB0") << CodePage::ID::CP273 << 0xB0 << 0x00A2; // Python cp273
    QTest::newRow("CP273 0xFC") << CodePage::ID::CP273 << 0xFC << 0x005D; // Python cp273
    QTest::newRow("CP273 0x7B") << CodePage::ID::CP273 << 0x7B << 0x0023; // Python cp273
    QTest::newRow("CP273 0x5B") << CodePage::ID::CP273 << 0x5B << 0x0024; // Python cp273
    QTest::newRow("CP277 0x47") << CodePage::ID::CP277 << 0x47 << 0x007D; // Java
    QTest::newRow("CP277 0x4F") << CodePage::ID::CP277 << 0x4F << 0x0021; // Java
    QTest::newRow("CP277 0x9C") << CodePage::ID::CP277 << 0x9C << 0x007B; // Java
    QTest::newRow("CP277 0xDC") << CodePage::ID::CP277 << 0xDC << 0x007E; // Java
    QTest::newRow("CP278 0x43") << CodePage::ID::CP278 << 0x43 << 0x007B; // Java
    QTest::newRow("CP278 0x47") << CodePage::ID::CP278 << 0x47 << 0x007D; // Java
    QTest::newRow("CP278 0x9F") << CodePage::ID::CP278 << 0x9F << 0x005D; // Java
    QTest::newRow("CP278 0xEC") << CodePage::ID::CP278 << 0xEC << 0x0040; // Java
    QTest::newRow("CP280 0x44") << CodePage::ID::CP280 << 0x44 << 0x007B; // Java
    QTest::newRow("CP280 0x48") << CodePage::ID::CP280 << 0x48 << 0x005C; // Java
    QTest::newRow("CP280 0xA1") << CodePage::ID::CP280 << 0xA1 << 0x00EC; // Java
    QTest::newRow("CP280 0xE0") << CodePage::ID::CP280 << 0xE0 << 0x00E7; // Java
    QTest::newRow("CP284 0x49") << CodePage::ID::CP284 << 0x49 << 0x00A6; // Java
    QTest::newRow("CP284 0x4A") << CodePage::ID::CP284 << 0x4A << 0x005B; // Java
    QTest::newRow("CP284 0x7B") << CodePage::ID::CP284 << 0x7B << 0x00D1; // Java
    QTest::newRow("CP284 0xBD") << CodePage::ID::CP284 << 0xBD << 0x007E; // Java
    QTest::newRow("CP285 0x5F") << CodePage::ID::CP285 << 0x5F << 0x00AC; // Java
    QTest::newRow("CP285 0x7B") << CodePage::ID::CP285 << 0x7B << 0x0023; // Java
    QTest::newRow("CP285 0xB1") << CodePage::ID::CP285 << 0xB1 << 0x005B; // Java
    QTest::newRow("CP285 0xBD") << CodePage::ID::CP285 << 0xBD << 0x00A8; // Java
    QTest::newRow("CP297 0x44") << CodePage::ID::CP297 << 0x44 << 0x0040; // Java
    QTest::newRow("CP297 0x48") << CodePage::ID::CP297 << 0x48 << 0x005C; // Java
    QTest::newRow("CP297 0xA0") << CodePage::ID::CP297 << 0xA0 << 0x0060; // Java
    QTest::newRow("CP297 0xE0") << CodePage::ID::CP297 << 0xE0 << 0x00E7; // Java
    QTest::newRow("CP500 0xAD") << CodePage::ID::CP500 << 0xAD << 0x00DD; // Python cp500
    QTest::newRow("CP500 0xB0") << CodePage::ID::CP500 << 0xB0 << 0x00A2; // Python cp500
    QTest::newRow("CP500 0xBA") << CodePage::ID::CP500 << 0xBA << 0x00AC; // Python cp500
    QTest::newRow("CP500 0xBD") << CodePage::ID::CP500 << 0xBD << 0x00A8; // Python cp500
    QTest::newRow("CP870 0x44") << CodePage::ID::CP870 << 0x44 << 0x0163; // Java
    QTest::newRow("CP870 0x64") << CodePage::ID::CP870 << 0x64 << 0x02DD; // Java
    QTest::newRow("CP870 0xB3") << CodePage::ID::CP870 << 0xB3 << 0x0162; // Java
    QTest::newRow("CP870 0xFE") << CodePage::ID::CP870 << 0xFE << 0x00DA; // Java
    QTest::newRow("CP420 0x42") << CodePage::ID::CP420 << 0x42 << 0x0651; // Java
    QTest::newRow("CP420 0x43") << CodePage::ID::CP420 << 0x43 << 0xFE7D; // Java
    QTest::newRow("CP420 0x9D") << CodePage::ID::CP420 << 0x9D << 0xFECC; // Java
    QTest::newRow("CP420 0xFE") << CodePage::ID::CP420 << 0xFE << 0x0669; // Java
    QTest::newRow("CP424 0x71") << CodePage::ID::CP424 << 0x71 << 0x05EA; // Python cp424
    QTest::newRow("CP424 0x74") << CodePage::ID::CP424 << 0x74 << 0x00A0; // Python cp424
    QTest::newRow("CP424 0xB5") << CodePage::ID::CP424 << 0xB5 << 0x00A7; // Python cp424
    QTest::newRow("CP424 0xFA") << CodePage::ID::CP424 << 0xFA << 0x00B3; // Python cp424
    QTest::newRow("CP838 0x41") << CodePage::ID::CP838 << 0x41 << 0x00A0; // Java
    QTest::newRow("CP838 0x42") << CodePage::ID::CP838 << 0x42 << 0x0E01; // Java
    QTest::newRow("CP838 0x90") << CodePage::ID::CP838 << 0x90 << 0x0E5A; // Java
    QTest::newRow("CP838 0xFB") << CodePage::ID::CP838 << 0xFB << 0x0E4C; // Java
}

void TestCodePage::testIbmAnchors() {
    QFETCH(CodePage::ID, id);
    QFETCH(int, byte);
    QFETCH(int, unicode);
    CodePage cp(id);
    QCOMPARE(int(cp.toUnicode(static_cast<uint8_t>(byte)).unicode()), unicode);
    QCOMPARE(int(cp.fromUnicode(QChar(static_cast<char16_t>(unicode)))), byte);
}

// Every mapped graphic byte encodes back to a byte showing the same
// character (CP838 has five Thai marks at two bytes each)
void TestCodePage::testGraphicRangeRoundTrips() {
    for (auto id : CodePage::supportedCodePages()) {
        CodePage cp(id);
        for (int b = 0x40; b < 0x100; ++b) {
            const QChar ch = cp.toUnicode(static_cast<uint8_t>(b));
            if (ch == QChar(0xFFFD)) continue;
            const uint8_t back = cp.fromUnicode(ch);
            QVERIFY2(cp.toUnicode(back) == ch,
                     qPrintable(QString("code page %1: 0x%2 -> U+%3 -> 0x%4")
                                    .arg(static_cast<int>(id))
                                    .arg(b, 2, 16, QChar('0'))
                                    .arg(int(ch.unicode()), 4, 16, QChar('0'))
                                    .arg(back, 2, 16, QChar('0'))));
        }
    }
}

// The five CP838 tone marks sit at two bytes each; IBM's tables pick one
// encoding per mark (expected values from Java's IBM-Thai encoder)
void TestCodePage::testThaiToneMarksEncodeLikeIbm() {
    CodePage cp(CodePage::ID::CP838);
    QCOMPARE(int(cp.fromUnicode(QChar(0x0E48))), 0xED);
    QCOMPARE(int(cp.fromUnicode(QChar(0x0E49))), 0xEE);
    QCOMPARE(int(cp.fromUnicode(QChar(0x0E4A))), 0xEF);
    QCOMPARE(int(cp.fromUnicode(QChar(0x0E4B))), 0xFA);
    QCOMPARE(int(cp.fromUnicode(QChar(0x0E4C))), 0xFB);
}

// Unmapped graphic bytes must not become the encoding of NUL
void TestCodePage::testNulEncodesToNul() {
    for (auto id : CodePage::supportedCodePages()) {
        CodePage cp(id);
        QCOMPARE(int(cp.fromUnicode(QChar(0x0000))), 0x00);
    }
}

void TestCodePage::testUnmappableUnicode() {
    CodePage cp(CodePage::ID::CP037);

    // CJK character - not in any EBCDIC code page
    uint8_t result = cp.fromUnicode(QChar(0x4E2D)); // 中
    QCOMPARE(result, static_cast<uint8_t>(0x40)); // should map to space
}

void TestCodePage::testReplacementChar() {
    CodePage cp(CodePage::ID::CP037);

    // Unmapped EBCDIC bytes (in the gaps) should return replacement character
    // 0x00 maps to 0x0000 (NUL) - special case
    QCOMPARE(cp.toUnicode(0x00), QChar(0x0000));

    // Check a byte that maps to 0 in the table but isn't byte 0
    // These are control bytes - they map to themselves (< 0x40)
    QCOMPARE(cp.toUnicode(0x01).unicode(), static_cast<char16_t>(0x0001));
}

void TestCodePage::testLettersConsistentAcrossCodePages() {
    // Per IBM spec, A-Z and a-z are at the same EBCDIC positions in ALL code pages
    QList<CodePage::ID> pages = CodePage::supportedCodePages();
    CodePage ref(CodePage::ID::CP037);

    for (auto id : pages) {
        if (id == CodePage::ID::CP037) continue;
        CodePage cp(id);

        // Check lowercase a-i (0x81-0x89)
        for (uint8_t b = 0x81; b <= 0x89; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
        // Check lowercase j-r (0x91-0x99)
        for (uint8_t b = 0x91; b <= 0x99; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
        // Check lowercase s-z (0xA2-0xA9)
        for (uint8_t b = 0xA2; b <= 0xA9; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
        // Check uppercase A-Z (0xC1-0xC9, 0xD1-0xD9, 0xE2-0xE9)
        for (uint8_t b = 0xC1; b <= 0xC9; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
        for (uint8_t b = 0xD1; b <= 0xD9; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
        for (uint8_t b = 0xE2; b <= 0xE9; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
    }
}

void TestCodePage::testDigitsConsistentAcrossCodePages() {
    QList<CodePage::ID> pages = CodePage::supportedCodePages();
    CodePage ref(CodePage::ID::CP037);

    for (auto id : pages) {
        if (id == CodePage::ID::CP037) continue;
        CodePage cp(id);
        for (uint8_t b = 0xF0; b <= 0xF9; ++b) {
            QCOMPARE(cp.toUnicode(b), ref.toUnicode(b));
        }
    }
}

QTEST_MAIN(TestCodePage)
#include "test_codepage.moc"
