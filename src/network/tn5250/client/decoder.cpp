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
    //   [0..1]   Big-endian record length (includes these two bytes)
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
        if (recLen < 6) {
            // Corrupt stream: drop one byte to resynchronize
            m_buffer.remove(0, 1);
            continue;
        }
        if (m_buffer.size() < recLen) {
            // Wait for full record
            break;
        }

        // We have a full record (recLen includes the 2-byte length field)
        QByteArray rec = m_buffer.mid(0, recLen);
        m_buffer.remove(0, recLen);

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
        // rec.size() == recLen (includes the 2-byte length field)
        int payloadLen = rec.size() - payloadStart;
        if (payloadLen < 0) {
            payloadLen = 0;
        }
        QByteArray payload = payloadLen > 0 ? rec.mid(payloadStart, payloadLen) : QByteArray();

        // For opcodes that carry display data, consume ESC sequences and emit display stream
        // Opcodes seen:
        //   0x02 Output Only, 0x03 Put/Get, 0x05 Restore Screen
        if (opcode == 0x04) { // Save Screen
            emit saveScreenRequested();
            continue;
        }
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
                    if (cc == 0x20) { // Clear Unit Alternate (27x132 mode)
                        emit clearScreenAlternateRequested();
                        i += 2;
                        continue;
                    }
                    if (cc == 0x21) { // Write Error Code
                        // Format: ESC 0x21 [orders/data until next ESC or end]
                        // All data between the command and the next ESC is written to the error line.
                        int j = i + 2; // skip ESC + cmd
                        QByteArray errData;
                        while (j < payload.size()) {
                            uint8_t ob = static_cast<uint8_t>(payload[j]);
                            if (ob == 0x04) break; // next ESC begins new command
                            errData.append(static_cast<char>(ob));
                            j++;
                        }
                        emit writeErrorCodeRequested(errData);
                        i = j;
                        continue;
                    }
                    if (cc == 0x23) { // Roll
                        // Format: ESC 0x23 ctrl1 ctrl2
                        if (i + 3 < payload.size()) {
                            uint8_t ctrl1 = static_cast<uint8_t>(payload[i + 2]);
                            uint8_t ctrl2 = static_cast<uint8_t>(payload[i + 3]);
                            bool up = (ctrl1 & 0x80) != 0; // bit 0 = direction (1=up, 0=down)
                            uint8_t lineCount = ctrl2 & 0x1F; // bits 3-7 = number of lines
                            // Top and bottom row operands follow in some variants,
                            // but typically ctrl1 bits 1-7 = top line, ctrl2 bits 0-4 = count
                            // IBM spec: after ESC 0x23, two operand bytes follow
                            uint8_t topRow = 0;
                            uint8_t botRow = 0;
                            if (i + 5 < payload.size()) {
                                topRow = static_cast<uint8_t>(payload[i + 4]) - 1; // 1-based to 0-based
                                botRow = static_cast<uint8_t>(payload[i + 5]) - 1;
                                i += 6;
                            } else {
                                i += 4;
                            }
                            emit rollRequested(topRow, botRow, lineCount, up);
                        } else {
                            i += 4;
                        }
                        continue;
                    }
                    if (cc == 0x42) { // Read Input Fields
                        emit commandReceived(TN5250Command::READ_INPUT_FIELDS, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == 0x50) { // Clear Format Table
                        emit clearFormatTableRequested();
                        i += 2;
                        continue;
                    }
                    if (cc == 0x52) { // Read MDT Fields — host requests input fields
                        // Format: ESC 0x52 ctrl1 ctrl2
                        emit commandReceived(TN5250Command::READ_MDT_FIELDS, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == 0x72) { // Read Immediate
                        emit commandReceived(TN5250Command::READ_IMMEDIATE, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == 0x11) { // Write To Display
                        // Requires two control bytes after CC
                        if (i + 3 >= payload.size()) {
                            break; // truncated WTD header
                        }
                        uint8_t ctrl1 = static_cast<uint8_t>(payload[i + 2]);
                        uint8_t ctrl2 = static_cast<uint8_t>(payload[i + 3]);
                        // Emit full CC bytes for processing
                        emit controlCharactersReceived(ctrl1, ctrl2);
                        // Keep legacy signal for backward compat
                        if (ctrl2 & 0x08) {
                            emit keyboardUnlockRequested();
                        }
                        int j = i + 4; // start of orders/data
                        // Collect until next ESC or end.
                        // We must parse known display orders so their operand bytes
                        // (which may be 0x01) are not mistaken for SOH markers.
                        while (j < payload.size()) {
                            uint8_t ob = static_cast<uint8_t>(payload[j]);
                            if (ob == 0x04) { // next ESC begins new command
                                break;
                            }
                            if (ob == 0x01) {
                                // SOH: parse header block and emit data
                                if (j + 1 >= payload.size()) {
                                    j = payload.size();
                                    break;
                                }
                                uint8_t sohLen = static_cast<uint8_t>(payload[j + 1]);
                                if (sohLen >= 4) {
                                    uint8_t errorRow = (j + 5 < payload.size()) ? static_cast<uint8_t>(payload[j + 5]) : 0;
                                    uint8_t ckm1 = (sohLen >= 5 && j + 6 < payload.size()) ? static_cast<uint8_t>(payload[j + 6]) : 0;
                                    uint8_t ckm2 = (sohLen >= 6 && j + 7 < payload.size()) ? static_cast<uint8_t>(payload[j + 7]) : 0;
                                    uint8_t ckm3 = (sohLen >= 7 && j + 8 < payload.size()) ? static_cast<uint8_t>(payload[j + 8]) : 0;
                                    emit sohReceived(errorRow, ckm1, ckm2, ckm3);
                                }
                                j += 2 + sohLen;
                                continue;
                            }
                            // Known fixed-length display orders: copy order + operands
                            // to display without checking operand bytes for SOH.
                            if (ob == 0x11 || ob == 0x13 || ob == 0x14) {
                                // SBA (0x11), IC (0x13), MC (0x14): 3 bytes (order + row + col)
                                int n = qMin(3, payload.size() - j);
                                display.append(payload.mid(j, n));
                                j += n;
                                continue;
                            }
                            if (ob == 0x02 || ob == 0x03) {
                                // RA (0x02), EA (0x03): 4 bytes (order + row + col + char)
                                int n = qMin(4, payload.size() - j);
                                display.append(payload.mid(j, n));
                                j += n;
                                continue;
                            }
                            if (ob == 0x10) {
                                // TD (Transparent Data): 2 + count bytes
                                if (j + 1 < payload.size()) {
                                    int tdLen = static_cast<uint8_t>(payload[j + 1]);
                                    int n = qMin(2 + tdLen, payload.size() - j);
                                    display.append(payload.mid(j, n));
                                    j += n;
                                } else {
                                    display.append(static_cast<char>(ob));
                                    j++;
                                }
                                continue;
                            }
                            if (ob == 0x15) {
                                // WDSF (Write to Display Structured Field): variable length
                                // Format: 0x15 [lenHi lenLo] [type] [data...]
                                // Length includes the 2 length bytes themselves.
                                // Skip entirely — not yet rendered.
                                if (j + 2 < payload.size()) {
                                    int wdsfLen = (static_cast<uint8_t>(payload[j + 1]) << 8) |
                                                   static_cast<uint8_t>(payload[j + 2]);
                                    if (wdsfLen < 2) wdsfLen = 2;
                                    j += 1 + wdsfLen; // order byte + structured field
                                    if (j > payload.size()) j = payload.size();
                                } else {
                                    j = payload.size();
                                }
                                continue;
                            }
                            if (ob == 0x1D) {
                                // SF (Start Field): variable length
                                // Format: 0x1D FFW1 FFW2 [FCW pairs...] attr lenHi lenLo [fieldData]
                                int k = j + 1; // past order code
                                if (k + 1 < payload.size()) k += 2; // FFW1 + FFW2
                                else { k = payload.size(); }
                                // Skip FCW pairs: bytes with bits 7-5 != 001 (not in 0x20-0x3F range)
                                while (k + 1 < payload.size() &&
                                       (static_cast<uint8_t>(payload[k]) & 0xE0) != 0x20) {
                                    k += 2; // FCW is 2 bytes
                                }
                                if (k < payload.size()) k++; // attribute byte
                                // 2-byte field length (big-endian)
                                int fieldLen = 0;
                                if (k + 1 < payload.size()) {
                                    fieldLen = (static_cast<uint8_t>(payload[k]) << 8) |
                                               static_cast<uint8_t>(payload[k + 1]);
                                    k += 2;
                                }
                                k += fieldLen; // skip field data
                                if (k > payload.size()) k = payload.size();
                                int n = k - j;
                                display.append(payload.mid(j, n));
                                j = k;
                                continue;
                            }
                            // Data byte or unrecognized order: copy through
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
                    // SOH outside WTD: parse and skip block
                    if (i + 1 >= payload.size()) {
                        break;
                    }
                    uint8_t sohLen = static_cast<uint8_t>(payload[i + 1]);
                    if (sohLen >= 4) {
                        uint8_t errorRow = (i + 5 < payload.size()) ? static_cast<uint8_t>(payload[i + 5]) : 0;
                        uint8_t ckm1 = (sohLen >= 5 && i + 6 < payload.size()) ? static_cast<uint8_t>(payload[i + 6]) : 0;
                        uint8_t ckm2 = (sohLen >= 6 && i + 7 < payload.size()) ? static_cast<uint8_t>(payload[i + 7]) : 0;
                        uint8_t ckm3 = (sohLen >= 7 && i + 8 < payload.size()) ? static_cast<uint8_t>(payload[i + 8]) : 0;
                        emit sohReceived(errorRow, ckm1, ckm2, ckm3);
                    }
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
