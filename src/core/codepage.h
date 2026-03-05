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
};

} // namespace core
