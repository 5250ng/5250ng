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
