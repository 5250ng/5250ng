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

#include "host_data_stream.h"

namespace hostserver {

QByteArray HostDataStream::buildPacket(uint8_t clientAttr,
                                       uint8_t serverAttr,
                                       ServerID serverId,
                                       uint32_t correlation,
                                       uint16_t templateLength,
                                       uint16_t reqRepId,
                                       const QByteArray &payload) {
    uint32_t totalLen = HEADER_SIZE + payload.size();
    QByteArray pkt;
    pkt.reserve(static_cast<int>(totalLen));

    writeU32(pkt, totalLen);
    pkt.append(static_cast<char>(clientAttr));
    pkt.append(static_cast<char>(serverAttr));
    writeU16(pkt, static_cast<uint16_t>(serverId));
    writeU32(pkt, 0); // CS Instance
    writeU32(pkt, correlation);
    writeU16(pkt, templateLength);
    writeU16(pkt, reqRepId);
    pkt.append(payload);

    return pkt;
}

bool HostDataStream::parseHeader(const QByteArray &data,
                                 uint32_t &length,
                                 uint8_t &clientAttr,
                                 uint8_t &serverAttr,
                                 uint16_t &serverId,
                                 uint32_t &csInstance,
                                 uint32_t &correlation,
                                 uint16_t &templateLength,
                                 uint16_t &reqRepId) {
    if (data.size() < HEADER_SIZE) return false;

    length = readU32(data, 0);
    clientAttr = static_cast<uint8_t>(data[4]);
    serverAttr = static_cast<uint8_t>(data[5]);
    serverId = readU16(data, 6);
    csInstance = readU32(data, 8);
    correlation = readU32(data, 12);
    templateLength = readU16(data, 16);
    reqRepId = readU16(data, 18);
    return true;
}

uint32_t HostDataStream::packetLength(const QByteArray &data) {
    if (data.size() < 4) return 0;
    return readU32(data, 0);
}

uint16_t HostDataStream::readU16(const QByteArray &data, int offset) {
    if (offset + 2 > data.size()) return 0;
    return static_cast<uint16_t>(
        (static_cast<uint8_t>(data[offset]) << 8) |
        static_cast<uint8_t>(data[offset + 1]));
}

uint32_t HostDataStream::readU32(const QByteArray &data, int offset) {
    if (offset + 4 > data.size()) return 0;
    return (static_cast<uint32_t>(static_cast<uint8_t>(data[offset])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 2])) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 3]));
}

uint64_t HostDataStream::readU64(const QByteArray &data, int offset) {
    if (offset + 8 > data.size()) return 0;
    return (static_cast<uint64_t>(readU32(data, offset)) << 32) |
           static_cast<uint64_t>(readU32(data, offset + 4));
}

void HostDataStream::writeU16(QByteArray &buf, uint16_t v) {
    buf.append(static_cast<char>((v >> 8) & 0xFF));
    buf.append(static_cast<char>(v & 0xFF));
}

void HostDataStream::writeU32(QByteArray &buf, uint32_t v) {
    buf.append(static_cast<char>((v >> 24) & 0xFF));
    buf.append(static_cast<char>((v >> 16) & 0xFF));
    buf.append(static_cast<char>((v >> 8) & 0xFF));
    buf.append(static_cast<char>(v & 0xFF));
}

void HostDataStream::writeU64(QByteArray &buf, uint64_t v) {
    writeU32(buf, static_cast<uint32_t>(v >> 32));
    writeU32(buf, static_cast<uint32_t>(v & 0xFFFFFFFF));
}

QByteArray HostDataStream::buildLLCP(uint16_t cp, const QByteArray &data) {
    QByteArray field;
    uint32_t ll = static_cast<uint32_t>(data.size()) + 6;
    writeU32(field, ll);
    writeU16(field, cp);
    field.append(data);
    return field;
}

QByteArray HostDataStream::findCodePoint(const QByteArray &data, int offset, uint16_t cp) {
    if (offset < 0) return {};
    while (offset + 6 <= data.size()) {
        uint32_t ll = readU32(data, offset);
        if (ll < 6 || static_cast<qint64>(ll) > data.size() - offset) break;
        uint16_t thisCp = readU16(data, offset + 4);
        if (thisCp == cp) {
            return data.mid(offset + 6, static_cast<int>(ll - 6));
        }
        offset += static_cast<int>(ll);
    }
    return {};
}

QByteArray HostDataStream::toUtf16BE(const QString &str) {
    QByteArray result;
    result.reserve(str.size() * 2);
    for (int i = 0; i < str.size(); ++i) {
        uint16_t codeUnit = str.at(i).unicode();
        result.append(static_cast<char>((codeUnit >> 8) & 0xFF));
        result.append(static_cast<char>(codeUnit & 0xFF));
    }
    return result;
}

QString HostDataStream::fromUtf16BE(const QByteArray &data) {
    QString result;
    result.reserve(data.size() / 2);
    for (int i = 0; i + 1 < data.size(); i += 2) {
        uint16_t codeUnit = static_cast<uint16_t>(
            (static_cast<uint8_t>(data[i]) << 8) |
            static_cast<uint8_t>(data[i + 1]));
        result.append(QChar(codeUnit));
    }
    return result;
}

// The byte JTOpen's SignonConverter gives each character it accepts in a
// user ID or password, or -1. The national characters share the bytes of
// $ # @, which is how the host stores them in names
static int signonByte(char16_t c) {
    static constexpr char kPunct[] = "\"#$%&'()*+,-./0123456789:;<=>?@";
    static constexpr uint8_t kPunctBytes[] = {0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D, 0x4D, 0x5D, 0x5C, 0x4E, 0x6B,
                                              0x60, 0x4B, 0x61, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
                                              0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F, 0x7C};
    for (int i = 0; kPunct[i]; ++i)
        if (c == static_cast<char16_t>(kPunct[i])) return kPunctBytes[i];
    if (c >= u'A' && c <= u'I') return 0xC1 + (c - u'A');
    if (c >= u'J' && c <= u'R') return 0xD1 + (c - u'J');
    if (c >= u'S' && c <= u'Z') return 0xE2 + (c - u'S');
    if (c >= u'a' && c <= u'i') return 0x81 + (c - u'a');
    if (c >= u'j' && c <= u'r') return 0x91 + (c - u'j');
    if (c >= u's' && c <= u'z') return 0xA2 + (c - u's');
    switch (c) {
    case u'!': return 0x5A;
    case u'_': return 0x6D;
    case 0x00A3: return 0x7B; // pound sign (CCSID 297)
    case 0x00A5: return 0x5B; // yen sign (CCSID 281)
    case 0x00A7: return 0x7C; // section sign (CCSID 273)
    case 0x00C4: return 0x7B; // A diaeresis (CCSID 278)
    case 0x00C5: return 0x5B; // A ring (CCSID 277)
    case 0x00C6: return 0x7B; // AE (CCSID 277)
    case 0x00D0: return 0x7C; // Eth (CCSID 871)
    case 0x00D1: return 0x7B; // N tilde (CCSID 284)
    case 0x00D6: return 0x7C; // O diaeresis (CCSID 278)
    case 0x00D8: return 0x7C; // O stroke (CCSID 277)
    case 0x00E0: return 0x7C; // a grave (CCSID 297)
    case 0x0130: return 0x5B; // I dot (CCSID 905)
    case 0x015E: return 0x7C; // S cedilla (CCSID 905)
    default: return -1;
    }
}

QString HostDataStream::userIdError(const QString &userId) {
    // Counted upper-cased, as sent: toUpper() can lengthen it (ß -> SS)
    const QString upper = userId.toUpper();
    if (upper.size() > 10) return QStringLiteral("User ID longer than 10 characters");
    for (QChar ch : upper) {
        if (signonByte(ch.unicode()) < 0)
            return QStringLiteral("User ID character U+%1 is not valid")
                .arg(QString::number(ch.unicode(), 16).toUpper().rightJustified(4, QLatin1Char('0')));
    }
    return {};
}

QByteArray HostDataStream::encodeUserId(const QString &userId) {
    if (!userIdError(userId).isEmpty()) return {};
    QByteArray ebcdic;
    for (QChar ch : userId.toUpper()) ebcdic.append(static_cast<char>(signonByte(ch.unicode())));
    return ebcdic.leftJustified(10, static_cast<char>(0x40));
}

QString HostDataStream::decodeUserId(const QByteArray &ebcdic) {
    // JTOpen's SignonConverter.byteArrayToCharArray: blank, $ _ # @, A-Z
    // and 0-9; any other byte fails
    QString result;
    for (char b : ebcdic) {
        const uint8_t c = static_cast<uint8_t>(b);
        if (c == 0x40) result.append(QLatin1Char(' '));
        else if (c == 0x5B) result.append(QLatin1Char('$'));
        else if (c == 0x6D) result.append(QLatin1Char('_'));
        else if (c == 0x7B) result.append(QLatin1Char('#'));
        else if (c == 0x7C) result.append(QLatin1Char('@'));
        else if (c >= 0xC1 && c <= 0xC9) result.append(QChar(u'A' + (c - 0xC1)));
        else if (c >= 0xD1 && c <= 0xD9) result.append(QChar(u'J' + (c - 0xD1)));
        else if (c >= 0xE2 && c <= 0xE9) result.append(QChar(u'S' + (c - 0xE2)));
        else if (c >= 0xF0 && c <= 0xF9) result.append(QChar(u'0' + (c - 0xF0)));
        else return {};
    }
    return result;
}

} // namespace hostserver
