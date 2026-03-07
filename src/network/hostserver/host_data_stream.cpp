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
#include "core/codepage.h"

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
    while (offset + 6 <= data.size()) {
        uint32_t ll = readU32(data, offset);
        if (ll < 6 || offset + static_cast<int>(ll) > data.size()) break;
        uint16_t thisCp = readU16(data, offset + 4);
        if (thisCp == cp) {
            return data.mid(offset + 6, static_cast<int>(ll) - 6);
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

QByteArray HostDataStream::encodeUserId(const QString &userId) {
    core::CodePage cp(core::CodePage::ID::CP037);
    QString upper = userId.toUpper();
    QByteArray ebcdic = cp.fromUnicode(upper);
    if (ebcdic.size() > 10) {
        ebcdic.truncate(10);
    }
    while (ebcdic.size() < 10) {
        ebcdic.append(static_cast<char>(0x40));
    }
    return ebcdic;
}

} // namespace hostserver
