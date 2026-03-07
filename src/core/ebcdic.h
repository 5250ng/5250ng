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

#include "codepage.h"
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <memory>

namespace core {

// EBCDIC to UTF-8 conversion utilities
// Delegates to a CodePage when one is set; falls back to hardcoded CP037.

class EBCDIC {
  public:
    // Set the active code page (nullptr reverts to default CP037 tables)
    static void setCodePage(CodePage::ID id);
    static CodePage::ID activeCodePageId();

    // Convert EBCDIC byte to UTF-8 character
    static QChar ebcdicToChar(uint8_t ebcdic);

    // Convert UTF-8 character to EBCDIC byte
    static uint8_t charToEBCDIC(QChar ch);

    // Convert EBCDIC byte array to UTF-8 string
    static QString ebcdicToString(const QByteArray &ebcdic);

    // Convert UTF-8 string to EBCDIC byte array
    static QByteArray stringToEBCDIC(const QString &str);

    // Check if character is printable
    static bool isPrintable(uint8_t ebcdic);

  private:
    // Hardcoded CP037 tables (used as fallback)
    static const uint8_t EBCDIC_TO_ASCII[256];
    static const uint8_t ASCII_TO_EBCDIC[256];

    // Active code page (nullptr = use hardcoded CP037)
    static std::unique_ptr<CodePage> s_codePage;
};

} // namespace core
