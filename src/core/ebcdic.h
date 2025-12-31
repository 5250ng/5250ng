#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace core {

// EBCDIC to UTF-8 conversion utilities
// Based on standard EBCDIC code page 037 (US English)

class EBCDIC {
public:
    // Convert EBCDIC byte to UTF-8 character
    static QChar ebcdicToChar(uint8_t ebcdic);
    
    // Convert UTF-8 character to EBCDIC byte
    static uint8_t charToEBCDIC(QChar ch);
    
    // Convert EBCDIC byte array to UTF-8 string
    static QString ebcdicToString(const QByteArray& ebcdic);
    
    // Convert UTF-8 string to EBCDIC byte array
    static QByteArray stringToEBCDIC(const QString& str);
    
    // Check if character is printable
    static bool isPrintable(uint8_t ebcdic);
    
private:
    // EBCDIC to ASCII/UTF-8 conversion table (Code Page 037)
    static const uint8_t EBCDIC_TO_ASCII[256];
    static const uint8_t ASCII_TO_EBCDIC[256];
};

} // namespace core

