#include "decoder.h"
#include "logger/logger.h"

namespace tn5250::client {

Decoder::Decoder(QObject *parent)
    : QObject(parent), m_state(ParserState::WaitingForCommand) {}

void Decoder::parseData(const QByteArray &data) {
    // Accumulate data (records may arrive fragmented)
    m_buffer.append(data);

    // Parse zero or more RFC1205 GDS records
    // Record layout:
    //   [0..1]   Big-endian length (includes entire record after these two bytes)
    //   [2..3]   Record type, expected 0x12 0xA0 (General Data Stream)
    //   [4..5]   Reserved (0x00 0x00)
    //   [6]      Variable header length (n >= 4 when flags/opcode present)
    //   [7..]    Variable header (flags hi, flags lo, opcode, ... optional)
    //   [..]     Payload
    while (true) {
        if (m_buffer.size() < 6) {
            // Need at least fixed header
            break;
        }
        const uint8_t b0 = static_cast<uint8_t>(m_buffer[0]);
        const uint8_t b1 = static_cast<uint8_t>(m_buffer[1]);
        const int recLen = (static_cast<int>(b0) << 8) | static_cast<int>(b1);

        // Defensive checks
        if (recLen <= 0) {
            // Corrupt stream: drop one byte to resynchronize
            m_buffer.remove(0, 1);
            continue;
        }
        if (m_buffer.size() < recLen + 2) {
            // Wait for full record
            break;
        }

        // We have a full record
        QByteArray rec = m_buffer.mid(0, recLen + 2);
        m_buffer.remove(0, recLen + 2);

        if (rec.size() < 7) {
            emit parseError("TN5250: record too short");
            continue;
        }

        const uint8_t r2 = static_cast<uint8_t>(rec[2]);
        const uint8_t r3 = static_cast<uint8_t>(rec[3]);
        if (!(r2 == 0x12 && r3 == 0xA0)) {
            // Not a 0x12A0 GDS; fallback: forward as-is for legacy handlers
            emit rawScreenDataReceived(rec);
            continue;
        }

        const int varHdrStart = 6;
        const int varLen = static_cast<uint8_t>(rec[varHdrStart]);
        if (rec.size() < varHdrStart + 1 + varLen) {
            emit parseError("TN5250: incomplete variable header");
            continue;
        }
        if (varLen < 4) {
            emit parseError("TN5250: variable header too short");
            continue;
        }
        // flagsHi, flagsLo, opcode
        const uint8_t flagsHi = static_cast<uint8_t>(rec[varHdrStart + 1]);
        const uint8_t flagsLo = static_cast<uint8_t>(rec[varHdrStart + 2]);
        const uint8_t opcode = static_cast<uint8_t>(rec[varHdrStart + 3]);
        Q_UNUSED(flagsHi);
        Q_UNUSED(flagsLo);

        const int payloadStart = varHdrStart + varLen;
        int payloadLen = (recLen + 2) - payloadStart - 0; // +2 because len excludes first two bytes or includes? We used slice of length+2 already
        // Our 'rec' includes the 2-byte length + 'recLen' bytes; payload length is rec.size() - payloadStart
        payloadLen = rec.size() - payloadStart;
        if (payloadLen < 0) {
            payloadLen = 0;
        }
        QByteArray payload = payloadLen > 0 ? rec.mid(payloadStart, payloadLen) : QByteArray();

        // For opcodes that carry display data, consume ESC sequences and emit display stream
        // Opcodes seen:
        //   0x02 Output Only, 0x03 Put/Get, 0x05 Restore Screen
        if (opcode == 0x02 || opcode == 0x03 || opcode == 0x05) {
            QByteArray display;
            // Parse payload: sequences of ESC 0x04 CC (command code), where
            //   CC=0x40 -> Clear Unit
            //   CC=0x11 -> Write To Display, followed by 2 control bytes, then orders/data stream
            // Orders may include SOH 0x01 [len] [len bytes] that we should skip
            for (int i = 0; i < payload.size();) {
                uint8_t ch = static_cast<uint8_t>(payload[i]);
                if (ch == 0x04) { // ESC
                    if (i + 1 >= payload.size()) {
                        break; // truncated ESC; stop
                    }
                    uint8_t cc = static_cast<uint8_t>(payload[i + 1]);
                    if (cc == 0x40) { // Clear Unit
                        emit clearScreenRequested();
                        i += 2;
                        continue;
                    }
                    if (cc == 0x52) { // Read MDT Fields — host requests input fields
                        // Format: ESC 0x52 ctrl1 ctrl2
                        emit commandReceived(TN5250Command::READ_MDT_FIELDS, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == 0x11) { // Write To Display
                        // Requires two control bytes after CC
                        if (i + 3 >= payload.size()) {
                            break; // truncated WTD header
                        }
                        // control byte 2: bit3 (0x08) = unlock keyboard
                        uint8_t ctrl2 = static_cast<uint8_t>(payload[i + 3]);
                        if (ctrl2 & 0x08) {
                            emit keyboardUnlockRequested();
                        }
                        int j = i + 4; // start of orders/data
                        // Collect until next ESC or end
                        while (j < payload.size()) {
                            uint8_t ob = static_cast<uint8_t>(payload[j]);
                            if (ob == 0x04) { // next ESC begins new command
                                break;
                            }
                            if (ob == 0x01) {
                                // SOH: skip header block
                                if (j + 1 >= payload.size()) {
                                    j = payload.size();
                                    break;
                                }
                                uint8_t sohLen = static_cast<uint8_t>(payload[j + 1]);
                                j += 2 + sohLen;
                                continue;
                            }
                            // Copy order/data byte
                            display.append(static_cast<char>(ob));
                            j++;
                        }
                        i = j;
                        continue;
                    }
                    // Unknown ESC command: skip ESC + code
                    i += 2;
                    continue;
                }
                // Not in an ESC-command context; copy-through anything that looks like orders/data
                if (ch == 0x01) {
                    // SOH outside WTD: skip block
                    if (i + 1 >= payload.size()) {
                        break;
                    }
                    uint8_t sohLen = static_cast<uint8_t>(payload[i + 1]);
                    i += 2 + sohLen;
                    continue;
                }
                display.append(static_cast<char>(ch));
                i++;
            }
            if (!display.isEmpty()) {
                emit rawScreenDataReceived(display);
            }
            continue;
        }

        // Opcodes we don't act on yet: log and ignore
        logger::Logger::instance()->debug(
            QString("Decoder: GDS opcode 0x%1, payload %2 bytes")
                .arg(opcode, 2, 16, QChar('0'))
                .arg(payload.size()));
    }
}

void Decoder::reset() { m_state = ParserState::WaitingForCommand; }

} // namespace tn5250::client
