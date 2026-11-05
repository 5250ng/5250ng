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

#include "codepage.h"
#include <cstring>

namespace core {

#include "codepage_tables.inc"

CodePage::CodePage(ID id) : m_id(id) {
    // Initialize control character range (0x00-0x3F) - same for all code pages
    m_toUnicode.fill(0x0000);
    // Most control chars map to themselves or to control pictures
    for (int i = 0; i < 0x40; ++i) {
        m_toUnicode[i] = static_cast<uint16_t>(i);
    }
    // 0x40-0xFF: the graphic range, generated from IBM's CDRA mappings as
    // shipped in ICU (tools/gen_codepage_tables.py)
    switch (id) {
    case ID::CP037: loadGraphicRange(kCP037); break;
    case ID::CP273: loadGraphicRange(kCP273); break;
    case ID::CP277: loadGraphicRange(kCP277); break;
    case ID::CP278: loadGraphicRange(kCP278); break;
    case ID::CP280: loadGraphicRange(kCP280); break;
    case ID::CP284: loadGraphicRange(kCP284); break;
    case ID::CP285: loadGraphicRange(kCP285); break;
    case ID::CP297: loadGraphicRange(kCP297); break;
    case ID::CP500: loadGraphicRange(kCP500); break;
    case ID::CP870: loadGraphicRange(kCP870); break;
    case ID::CP420: loadGraphicRange(kCP420); break;
    case ID::CP424: loadGraphicRange(kCP424); break;
    case ID::CP838: loadGraphicRange(kCP838); break;
    }
}

QString CodePage::name() const { return codepageName(m_id); }

QString CodePage::codepageName(ID id) {
    switch (id) {
    case ID::CP037: return "037 - US/Canada";
    case ID::CP273: return "273 - Germany/Austria";
    case ID::CP277: return "277 - Denmark/Norway";
    case ID::CP278: return "278 - Finland/Sweden";
    case ID::CP280: return "280 - Italy";
    case ID::CP284: return "284 - Spain/Latin America";
    case ID::CP285: return "285 - UK";
    case ID::CP297: return "297 - France";
    case ID::CP500: return "500 - International";
    case ID::CP870: return "870 - Eastern Europe (Latin-2)";
    case ID::CP420: return "420 - Arabic";
    case ID::CP424: return "424 - Hebrew";
    case ID::CP838: return "838 - Thai";
    }
    return "Unknown";
}

// Keyboard types and character sets as in IBM's national language support
// tables (the same pairs tn5250j uses in KbdTypesCodePages).
QString CodePage::keyboardType(ID id) {
    switch (id) {
    case ID::CP037: return QStringLiteral("USB");
    case ID::CP273: return QStringLiteral("AGB");
    case ID::CP277: return QStringLiteral("DMB");
    case ID::CP278: return QStringLiteral("SWB");
    case ID::CP280: return QStringLiteral("ITB");
    case ID::CP284: return QStringLiteral("SPB");
    case ID::CP285: return QStringLiteral("UKB");
    case ID::CP297: return QStringLiteral("FAB");
    case ID::CP500: return QStringLiteral("INB");
    case ID::CP870: return QStringLiteral("ROB");
    case ID::CP420: return QStringLiteral("CLB");
    case ID::CP424: return QStringLiteral("NCB");
    case ID::CP838: return QStringLiteral("THB");
    }
    return QStringLiteral("USB");
}

int CodePage::characterSet(ID id) {
    switch (id) {
    case ID::CP870: return 959;  // Latin-2
    case ID::CP420: return 235;  // Arabic
    case ID::CP424: return 941;  // Hebrew
    case ID::CP838: return 1176; // Thai
    default:        return 697;  // Latin-1
    }
}

QList<CodePage::ID> CodePage::supportedCodePages() {
    return {
        ID::CP037, ID::CP273, ID::CP277, ID::CP278,
        ID::CP280, ID::CP284, ID::CP285, ID::CP297, ID::CP500,
        ID::CP870, ID::CP420, ID::CP424, ID::CP838
    };
}

bool CodePage::isKnownId(int id) {
    for (ID known : supportedCodePages()) {
        if (static_cast<int>(known) == id) return true;
    }
    return false;
}

QChar CodePage::toUnicode(uint8_t ebcdic) const {
    uint16_t u = m_toUnicode[ebcdic];
    if (u == 0 && ebcdic != 0) return QChar(0xFFFD); // replacement char
    return QChar(u);
}

uint8_t CodePage::fromUnicode(QChar ch) const {
    if (!m_reverseBuilt) buildReverseTable();
    uint16_t code = ch.unicode();
    uint8_t result = m_fromUnicodeTable[code];
    return (result != 0 || code == 0) ? result : 0x40;
}

QString CodePage::toUnicode(const QByteArray &ebcdic) const {
    QString result;
    result.reserve(ebcdic.size());
    for (uint8_t b : ebcdic) result.append(toUnicode(b));
    return result;
}

QByteArray CodePage::fromUnicode(const QString &str) const {
    QByteArray result;
    result.reserve(str.size());
    for (QChar ch : str) result.append(static_cast<char>(fromUnicode(ch)));
    return result;
}

void CodePage::buildReverseTable() const {
    m_fromUnicodeTable.fill(0x00);
    // Only the graphic range: 0x00-0x3F are 5250 orders and control codes,
    // and their identity entries would turn any character the national
    // table does not map (such as '#' or '$') into a control byte in the
    // field data. Such characters fall back to a space in fromUnicode().
    for (int i = 0x40; i < 256; ++i) {
        uint16_t u = m_toUnicode[i];
        // An unmapped byte (0) must not become the encoding of NUL
        if (u != 0) {
            m_fromUnicodeTable[u] = static_cast<uint8_t>(i);
        }
    }
    // Characters at two bytes encode to the byte IBM's tables choose
    if (m_id == ID::CP838) {
        for (const auto &pair : kCP838Preferred) {
            m_fromUnicodeTable[pair[0]] = static_cast<uint8_t>(pair[1]);
        }
    }
    m_reverseBuilt = true;
}

void CodePage::loadGraphicRange(const uint16_t *table) {
    for (int i = 0; i < 192; ++i) {
        m_toUnicode[0x40 + i] = table[i];
    }
}

} // namespace core
