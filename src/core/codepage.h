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

#pragma once

#include <QChar>
#include <QList>
#include <QString>
#include <QByteArray>
#include <cstdint>
#include <array>

namespace core {

// EBCDIC code page abstraction for multi-language support
// Each code page defines a bidirectional mapping between EBCDIC bytes and Unicode.
class CodePage {
  public:
    // Supported EBCDIC code pages
    enum class ID {
        CP037 = 37,    // US/Canada/Netherlands/Portugal/Brazil
        CP273 = 273,   // Germany/Austria
        CP277 = 277,   // Denmark/Norway
        CP278 = 278,   // Finland/Sweden
        CP280 = 280,   // Italy
        CP284 = 284,   // Spain/Latin America
        CP285 = 285,   // UK
        CP297 = 297,   // France
        CP500 = 500,   // International
        CP870 = 870,   // Eastern Europe (Latin-2)
        CP420 = 420,   // Arabic
        CP424 = 424,   // Hebrew
        CP838 = 838,   // Thai
    };

    explicit CodePage(ID id = ID::CP037);

    ID id() const { return m_id; }
    QString name() const;

    // Convert EBCDIC byte → Unicode char
    QChar toUnicode(uint8_t ebcdic) const;
    // Convert Unicode char → EBCDIC byte (0x40 if unmappable)
    uint8_t fromUnicode(QChar ch) const;

    // Bulk conversions
    QString toUnicode(const QByteArray &ebcdic) const;
    QByteArray fromUnicode(const QString &str) const;

    // Get list of supported code pages
    static QList<ID> supportedCodePages();
    static QString codepageName(ID id);
    // Returns true if the integer matches one of the supported code page IDs.
    static bool isKnownId(int id);

  private:
    ID m_id;
    // EBCDIC byte → Unicode codepoint (for bytes 0x40-0xFF)
    // Control chars (0x00-0x3F) are handled separately
    std::array<uint16_t, 256> m_toUnicode;
    // Build reverse table on demand
    mutable bool m_reverseBuilt = false;
    mutable std::array<uint8_t, 65536> m_fromUnicodeTable;
    void buildReverseTable() const;

    void initCP037();
    void initCP273();
    void initCP277();
    void initCP278();
    void initCP280();
    void initCP284();
    void initCP285();
    void initCP297();
    void initCP500();
    void initCP870();
    void initCP420();
    void initCP424();
    void initCP838();
};

} // namespace core
