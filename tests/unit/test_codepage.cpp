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
