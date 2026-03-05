#include "decoder.h"
#include "logger/logger.h"
#include "network/tn5250/protocol_constants.h"

namespace tn5250::client {

Decoder::Decoder(QObject *parent)
    : QObject(parent), m_state(ParserState::WaitingForCommand) {}

void Decoder::parseData(const QByteArray &data) {
    // Accumulate data (records may arrive fragmented)
    m_buffer.append(data);
    LOG_DEBUG(QString("[Decoder] parseData: received %1 bytes, buffer now %2 bytes")
        .arg(data.size()).arg(m_buffer.size()));

    // Parse zero or more RFC1205 GDS records
    // Record layout:
    //   [0..1]   Big-endian record length (includes these two bytes)
    //   [2..3]   Record type, expected 0x12 0xA0 (General Data Stream)
    //   [4..5]   Reserved (0x00 0x00)
    //   [6]      Variable header length (n >= 4 when flags/opcode present)
    //   [7..]    Variable header (flags hi, flags lo, opcode, ... optional)
    //   [..]     Payload
    using namespace tn5250::protocol;
    while (true) {
        if (m_buffer.size() < GDS_MIN_RECORD_LEN) {
            // Need at least fixed header
            break;
        }
        const uint8_t b0 = static_cast<uint8_t>(m_buffer[0]);
        const uint8_t b1 = static_cast<uint8_t>(m_buffer[1]);
        const int recLen = (static_cast<int>(b0) << 8) | static_cast<int>(b1);

        // Defensive checks
        if (recLen < GDS_MIN_RECORD_LEN) {
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
        LOG_DEBUG(QString("[Decoder] GDS record: len=%1 hex=%2")
            .arg(recLen).arg(QString::fromLatin1(rec.left(20).toHex())));

        if (rec.size() < 7) {
            emit parseError("TN5250: record too short");
            continue;
        }

        const uint8_t r2 = static_cast<uint8_t>(rec[2]);
        const uint8_t r3 = static_cast<uint8_t>(rec[3]);
        if (!(r2 == GDS_RECORD_TYPE_HI && r3 == GDS_RECORD_TYPE_LO)) {
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
        LOG_DEBUG(QString("[Decoder] GDS header: recLen=%1 varLen=%2 flags=0x%3%4 opcode=0x%5")
            .arg(recLen).arg(varLen)
            .arg(flagsHi, 2, 16, QChar('0')).arg(flagsLo, 2, 16, QChar('0'))
            .arg(opcode, 2, 16, QChar('0')));
        Q_UNUSED(flagsHi);
        Q_UNUSED(flagsLo);

        const int payloadStart = varHdrStart + varLen;
        // rec.size() == recLen (includes the 2-byte length field)
        int payloadLen = rec.size() - payloadStart;
        if (payloadLen < 0) {
            payloadLen = 0;
        }
        QByteArray payload = payloadLen > 0 ? rec.mid(payloadStart, payloadLen) : QByteArray();

        // Handle GDS opcodes that don't carry display data first
        if (opcode == GDS_OPCODE_SAVE_SCREEN) {
            LOG_DEBUG("[Decoder] opcode=0x04 SAVE_SCREEN");
            emit saveScreenRequested();
            continue;
        }
        if (opcode == GDS_OPCODE_INVITE) {
            LOG_DEBUG("[Decoder] opcode=0x01 INVITE");
            emit inviteReceived();
            continue;
        }
        if (opcode == GDS_OPCODE_CANCEL_INVITE) {
            LOG_DEBUG("[Decoder] opcode=0x0A CANCEL_INVITE");
            emit cancelInviteReceived();
            continue;
        }
        if (opcode == GDS_OPCODE_MSG_LIGHT_ON) {
            LOG_DEBUG("[Decoder] opcode=0x0B MSG_LIGHT_ON");
            emit messageLightOn();
            continue;
        }
        if (opcode == GDS_OPCODE_MSG_LIGHT_OFF) {
            LOG_DEBUG("[Decoder] opcode=0x0C MSG_LIGHT_OFF");
            emit messageLightOff();
            continue;
        }
        // For opcodes that carry display data, consume ESC sequences and emit display stream
        if (opcode == GDS_OPCODE_OUTPUT_ONLY || opcode == GDS_OPCODE_PUT_GET || opcode == GDS_OPCODE_RESTORE) {
            const char *opName = (opcode == GDS_OPCODE_OUTPUT_ONLY) ? "OUTPUT_ONLY" :
                                 (opcode == GDS_OPCODE_PUT_GET) ? "PUT_GET" : "RESTORE";
            LOG_DEBUG(QString("[Decoder] opcode=0x%1 %2, payload=%3 bytes")
                .arg(opcode, 2, 16, QChar('0')).arg(opName).arg(payload.size()));
            QByteArray display;
            // Parse payload: sequences of ESC 0x04 CC (command code), where
            //   CC=0x40 -> Clear Unit
            //   CC=0x11 -> Write To Display, followed by 2 control bytes, then orders/data stream
            // Orders may include SOH 0x01 [len] [len bytes] that we should skip
            for (int i = 0; i < payload.size();) {
                uint8_t ch = static_cast<uint8_t>(payload[i]);
                if (ch == ESC) {
                    if (i + 1 >= payload.size()) {
                        break; // truncated ESC; stop
                    }
                    uint8_t cc = static_cast<uint8_t>(payload[i + 1]);
                    if (cc == CC_CLEAR_UNIT) {
                        LOG_DEBUG("[Decoder] ESC 0x40 CLEAR_UNIT");
                        emit clearScreenRequested();
                        i += 2;
                        continue;
                    }
                    if (cc == CC_CLEAR_UNIT_ALTERNATE) {
                        LOG_DEBUG("[Decoder] ESC 0x20 CLEAR_UNIT_ALTERNATE (27x132)");
                        emit clearScreenAlternateRequested();
                        i += 2;
                        continue;
                    }
                    if (cc == CC_WRITE_ERROR_CODE) { // Write Error Code
                        // Format: ESC 0x21 [orders/data until next ESC or end]
                        // All data between the command and the next ESC is written to the error line.
                        int j = i + 2; // skip ESC + cmd
                        QByteArray errData;
                        while (j < payload.size()) {
                            uint8_t ob = static_cast<uint8_t>(payload[j]);
                            if (ob == ESC) break; // next ESC begins new command
                            errData.append(static_cast<char>(ob));
                            j++;
                        }
                        LOG_DEBUG(QString("[Decoder] ESC 0x21 WRITE_ERROR_CODE: %1 bytes")
                            .arg(errData.size()));
                        emit writeErrorCodeRequested(errData);
                        i = j;
                        continue;
                    }
                    if (cc == CC_ROLL) { // Roll
                        // Format: ESC 0x23 ctrl1 ctrl2
                        if (i + 3 < payload.size()) {
                            uint8_t ctrl1 = static_cast<uint8_t>(payload[i + 2]);
                            uint8_t ctrl2 = static_cast<uint8_t>(payload[i + 3]);
                            bool up = (ctrl1 & 0x80) != 0; // bit 0 = direction (1=up, 0=down)
                            uint8_t lineCount = ctrl2 & 0x1F; // bits 3-7 = number of lines
                            LOG_DEBUG(QString("[Decoder] ESC 0x23 ROLL: dir=%1 lines=%2 ctrl1=0x%3 ctrl2=0x%4")
                                .arg(up ? "up" : "down").arg(lineCount)
                                .arg(ctrl1, 2, 16, QChar('0')).arg(ctrl2, 2, 16, QChar('0')));
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
                    if (cc == CC_WRITE_ERROR_CODE_TO_WINDOW) {
                        // Same as Write Error Code (0x21) but for windowed context
                        int j = i + 2;
                        QByteArray errData;
                        while (j < payload.size()) {
                            uint8_t ob = static_cast<uint8_t>(payload[j]);
                            if (ob == ESC) break;
                            errData.append(static_cast<char>(ob));
                            j++;
                        }
                        LOG_DEBUG(QString("[Decoder] ESC 0x22 WRITE_ERROR_CODE_TO_WINDOW: %1 bytes")
                            .arg(errData.size()));
                        emit writeErrorCodeRequested(errData);
                        i = j;
                        continue;
                    }
                    if (cc == CC_READ_INPUT_FIELDS) {
                        LOG_DEBUG("[Decoder] ESC 0x42 READ_INPUT_FIELDS");
                        emit commandReceived(TN5250Command::READ_INPUT_FIELDS, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == CC_CLEAR_FORMAT_TABLE) {
                        LOG_DEBUG("[Decoder] ESC 0x50 CLEAR_FORMAT_TABLE");
                        emit clearFormatTableRequested();
                        i += 2;
                        continue;
                    }
                    if (cc == CC_READ_MDT_FIELDS) {
                        LOG_DEBUG("[Decoder] ESC 0x52 READ_MDT_FIELDS");
                        emit commandReceived(TN5250Command::READ_MDT_FIELDS, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == CC_READ_IMMEDIATE) {
                        LOG_DEBUG("[Decoder] ESC 0x72 READ_IMMEDIATE");
                        emit commandReceived(TN5250Command::READ_IMMEDIATE, QByteArray());
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == CC_READ_SCREEN || cc == CC_READ_SCREEN_ALT) {
                        LOG_DEBUG(QString("[Decoder] ESC 0x%1 READ_SCREEN").arg(cc, 2, 16, QChar('0')));
                        bool includeAttrs = (cc == CC_READ_SCREEN_ALT);
                        emit readScreenRequested(includeAttrs);
                        i += 4; // ESC + cmd + ctrl1 + ctrl2
                        continue;
                    }
                    if (cc == CC_WRITE_STRUCTURED_FIELD) {
                        // Write Structured Field: ESC 0xF3 + structured data
                        int j = i + 2;
                        QByteArray sfData;
                        while (j < payload.size()) {
                            uint8_t ob = static_cast<uint8_t>(payload[j]);
                            if (ob == ESC) break;
                            sfData.append(static_cast<char>(ob));
                            j++;
                        }
                        LOG_DEBUG(QString("[Decoder] ESC 0xF3 WRITE_STRUCTURED_FIELD: %1 bytes").arg(sfData.size()));
                        emit writeStructuredFieldReceived(sfData);
                        i = j;
                        continue;
                    }
                    if (cc == CC_WRITE_TO_DISPLAY) { // Write To Display
                        // Requires two control bytes after CC
                        if (i + 3 >= payload.size()) {
                            break; // truncated WTD header
                        }
                        uint8_t ctrl1 = static_cast<uint8_t>(payload[i + 2]);
                        uint8_t ctrl2 = static_cast<uint8_t>(payload[i + 3]);
                        LOG_DEBUG(QString("[Decoder] ESC 0x11 WRITE_TO_DISPLAY: CC1=0x%1 CC2=0x%2"
                            " [CC1: resetMDT=%3 clrInput=%4 lockKbd=%5]"
                            " [CC2: unlockKbd=%6 beep=%7 msgWait=%8]")
                            .arg(ctrl1, 2, 16, QChar('0')).arg(ctrl2, 2, 16, QChar('0'))
                            .arg((ctrl1 & 0x04) ? "Y" : "N")
                            .arg((ctrl1 & 0x02) ? "Y" : "N")
                            .arg((ctrl1 & 0x01) ? "Y" : "N")
                            .arg((ctrl2 & 0x08) ? "Y" : "N")
                            .arg((ctrl2 & 0x20) ? "Y" : "N")
                            .arg((ctrl2 & 0x80) ? "Y" : "N"));
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
                            if (ob == ESC) { // next ESC begins new command
                                break;
                            }
                            if (ob == SOH) {
                                // SOH: parse header block and emit data
                                if (j + 1 >= payload.size()) {
                                    j = payload.size();
                                    break;
                                }
                                uint8_t sohLen = static_cast<uint8_t>(payload[j + 1]);
                                LOG_DEBUG(QString("[Decoder] SOH block: len=%1").arg(sohLen));
                                if (sohLen >= 4) {
                                    uint8_t errorRow = (j + 5 < payload.size()) ? static_cast<uint8_t>(payload[j + 5]) : 0;
                                    uint8_t ckm1 = (sohLen >= 5 && j + 6 < payload.size()) ? static_cast<uint8_t>(payload[j + 6]) : 0;
                                    uint8_t ckm2 = (sohLen >= 6 && j + 7 < payload.size()) ? static_cast<uint8_t>(payload[j + 7]) : 0;
                                    uint8_t ckm3 = (sohLen >= 7 && j + 8 < payload.size()) ? static_cast<uint8_t>(payload[j + 8]) : 0;
                                    LOG_DEBUG(QString("[Decoder] SOH: errorRow=%1 cmdKeyMask=0x%2,0x%3,0x%4")
                                        .arg(errorRow).arg(ckm1, 2, 16, QChar('0'))
                                        .arg(ckm2, 2, 16, QChar('0')).arg(ckm3, 2, 16, QChar('0')));
                                    emit sohReceived(errorRow, ckm1, ckm2, ckm3);
                                }
                                j += 2 + sohLen;
                                continue;
                            }
                            // Known fixed-length display orders: copy order + operands
                            // to display without checking operand bytes for SOH.
                            if (ob == ORDER_SBA || ob == ORDER_IC || ob == ORDER_MC) {
                                // SBA, IC, MC: 3 bytes (order + row + col)
                                int n = qMin(3, payload.size() - j);
                                display.append(payload.mid(j, n));
                                j += n;
                                continue;
                            }
                            if (ob == ORDER_RA || ob == ORDER_EA) {
                                // RA, EA: 4 bytes (order + row + col + char)
                                int n = qMin(4, payload.size() - j);
                                display.append(payload.mid(j, n));
                                j += n;
                                continue;
                            }
                            if (ob == ORDER_TD) {
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
                            if (ob == ORDER_WDSF) {
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
                            if (ob == ORDER_SF) {
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
                    LOG_DEBUG(QString("[Decoder] Unknown ESC command: 0x%1")
                        .arg(cc, 2, 16, QChar('0')));
                    i += 2;
                    continue;
                }
                // Not in an ESC-command context; copy-through anything that looks like orders/data
                if (ch == SOH) {
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
                LOG_DEBUG(QString("[Decoder] emitting rawScreenDataReceived: %1 bytes of display data")
                    .arg(display.size()));
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
