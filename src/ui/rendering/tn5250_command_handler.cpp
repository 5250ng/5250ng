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

#include "tn5250_command_handler.h"
#include "logger/logger.h"
#include "tn5250/protocol_constants.h"
#include "utils/hex/hex.h"
#include <QApplication>
#include <vector>

namespace ui::rendering {

TN5250CommandHandler::TN5250CommandHandler(QObject *parent)
    : QObject(parent) {}

void TN5250CommandHandler::setDisplayWidget(ui::widgets::Q5250ScreenWidget *widget) {
    m_displayWidget = widget;
    delete m_renderer;
    m_renderer = widget ? new TN5250StreamRenderer(widget) : nullptr;
}

void TN5250CommandHandler::setSendToHostCallback(SendToHostFn fn) {
    m_sendToHost = std::move(fn);
}

void TN5250CommandHandler::setSendGDSCallback(SendGDSFn fn) {
    m_sendGDS = std::move(fn);
}

void TN5250CommandHandler::connectDecoder(tn5250::client::DecoderAdapter *parser) {
    if (!parser) return;
    connect(parser, &tn5250::client::DecoderAdapter::commandReceived,
            this, &TN5250CommandHandler::handleTN5250Command);
    connect(parser, &tn5250::client::DecoderAdapter::structuredFieldReceived,
            this, &TN5250CommandHandler::handleStructuredField);
    connect(parser, &tn5250::client::DecoderAdapter::rawScreenDataReceived,
            this, &TN5250CommandHandler::handleRawScreenData);
    connect(parser, &tn5250::client::DecoderAdapter::clearScreenRequested,
            this, &TN5250CommandHandler::onClearScreenRequested);
    connect(parser, &tn5250::client::DecoderAdapter::keyboardUnlockRequested,
            this, &TN5250CommandHandler::onKeyboardUnlockRequested);
    connect(parser, &tn5250::client::DecoderAdapter::controlCharactersReceived,
            this, &TN5250CommandHandler::onControlCharactersReceived);
    connect(parser, &tn5250::client::DecoderAdapter::sohReceived,
            this, &TN5250CommandHandler::onSohReceived);
    connect(parser, &tn5250::client::DecoderAdapter::rollRequested,
            this, &TN5250CommandHandler::onRollRequested);
    connect(parser, &tn5250::client::DecoderAdapter::writeErrorCodeRequested,
            this, &TN5250CommandHandler::onWriteErrorCode);
    connect(parser, &tn5250::client::DecoderAdapter::clearScreenAlternateRequested,
            this, &TN5250CommandHandler::onClearScreenAlternateRequested);
    connect(parser, &tn5250::client::DecoderAdapter::clearFormatTableRequested,
            this, &TN5250CommandHandler::onClearFormatTableRequested);
    connect(parser, &tn5250::client::DecoderAdapter::inviteReceived,
            this, &TN5250CommandHandler::onInviteReceived);
    connect(parser, &tn5250::client::DecoderAdapter::cancelInviteReceived,
            this, &TN5250CommandHandler::onCancelInviteReceived);
    connect(parser, &tn5250::client::DecoderAdapter::messageLightOn,
            this, &TN5250CommandHandler::onMessageLightOn);
    connect(parser, &tn5250::client::DecoderAdapter::messageLightOff,
            this, &TN5250CommandHandler::onMessageLightOff);
    connect(parser, &tn5250::client::DecoderAdapter::readScreenRequested,
            this, &TN5250CommandHandler::onReadScreenRequested);
    connect(parser, &tn5250::client::DecoderAdapter::writeStructuredFieldReceived,
            this, &TN5250CommandHandler::onWriteStructuredFieldReceived);
}

void TN5250CommandHandler::handleTN5250Command(tn5250::client::TN5250Command cmd,
                                                const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    logger::Logger::instance()->debug(
        QString("CommandHandler: Handling TN5250 command: %1, data size: %2")
            .arg(static_cast<int>(cmd))
            .arg(data.size()));

    switch (cmd) {
    case tn5250::client::TN5250Command::READ_MDT_FIELDS:
        m_readType = 0x52;
        if (m_displayWidget) m_displayWidget->setReadType(0x52);
        logger::Logger::instance()->debug("CommandHandler: READ_MDT_FIELDS - awaiting user AID key");
        break;

    case tn5250::client::TN5250Command::ERASE_WRITE:
    case tn5250::client::TN5250Command::ERASE_WRITE_ALTERNATE:
        logger::Logger::instance()->debug("CommandHandler: ERASE_WRITE variant (not used in data path)");
        break;

    case tn5250::client::TN5250Command::READ_MODIFY:
    case tn5250::client::TN5250Command::READ_MODIFY_WRITE:
        logger::Logger::instance()->debug("CommandHandler: READ_MODIFY command (not yet implemented)");
        break;

    case tn5250::client::TN5250Command::READ_INPUT_FIELDS:
        m_readType = 0x42;
        if (m_displayWidget) m_displayWidget->setReadType(0x42);
        logger::Logger::instance()->debug("CommandHandler: READ_INPUT_FIELDS - awaiting user AID key");
        break;

    case tn5250::client::TN5250Command::READ_IMMEDIATE:
        if (m_sendToHost) {
            m_sendToHost(buildFieldResponse(0xF1));
        }
        logger::Logger::instance()->debug("CommandHandler: READ_IMMEDIATE - sent immediate response");
        break;

    case tn5250::client::TN5250Command::WRITE_STRUCTURED_FIELD:
        logger::Logger::instance()->debug("CommandHandler: WRITE_STRUCTURED_FIELD (handled by structuredFieldReceived)");
        break;

    default:
        logger::Logger::instance()->warning(
            QString("CommandHandler: Unknown TN5250 command: %1")
                .arg(static_cast<int>(cmd)));
        break;
    }
}

void TN5250CommandHandler::handleStructuredField(tn5250::client::StructuredFieldType type,
                                                  const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    logger::Logger::instance()->debug(
        QString("CommandHandler: Handling structured field type: %1, data size: %2")
            .arg(static_cast<int>(type))
            .arg(data.size()));

    switch (type) {
    case tn5250::client::StructuredFieldType::OUTBOUND_5250_DS:
        if (data.size() >= 3 && m_renderer) {
            m_renderer->render(data.mid(3));
        }
        break;

    case tn5250::client::StructuredFieldType::SCS:
        logger::Logger::instance()->debug("CommandHandler: SCS structured field (not yet implemented)");
        break;

    default:
        logger::Logger::instance()->debug(
            QString("CommandHandler: Unhandled structured field type: %1")
                .arg(static_cast<int>(type)));
        break;
    }
}

void TN5250CommandHandler::handleRawScreenData(const QByteArray &data) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }

    if (!data.isEmpty()) {
        logger::Logger::instance()->debug(
            QString("CommandHandler: Handling raw screen data - %1 bytes")
                .arg(data.size()));
        std::vector<uint8_t> dumpBuf(reinterpret_cast<const uint8_t *>(data.constData()),
                                      reinterpret_cast<const uint8_t *>(data.constData()) + data.size());
        std::vector<std::string> hexLines = utils::hex::hexdump(dumpBuf);
        for (const std::string &line : hexLines) {
            logger::Logger::instance()->debug(QString::fromStdString(line));
        }

        if (m_renderer) {
            m_renderer->render(data);
        }
    }

    // Process deferred CC2 now that display data has been rendered
    // (or immediately if WTD had no display orders — the CC2 unlock
    //  must still fire to avoid leaving the keyboard permanently locked)
    if (m_pendingCC2) {
        processDeferredCC2(m_pendingCC2);
        m_pendingCC2 = 0;
    }
}

void TN5250CommandHandler::onClearScreenRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    m_displayWidget->screenBuffer()->clear();
    m_displayWidget->screenBuffer()->notifyCursor();
    m_displayWidget->setCursorBlinkRate(0);
}

void TN5250CommandHandler::onKeyboardUnlockRequested() {
    logger::Logger::instance()->debug("CommandHandler: Keyboard unlock requested by host");
    if (m_displayWidget && m_displayWidget->isVisible()) {
        m_displayWidget->setFocus();
    }
}

void TN5250CommandHandler::onControlCharactersReceived(uint8_t cc1, uint8_t cc2) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    auto *screen = m_displayWidget->screenBuffer();

    // CC1 top 3 bits (0xE0) control lock/MDT/field operations
    // per SA21-9247 / lib5250 reference implementation
    uint8_t cc1Hi = cc1 & 0xE0;
    LOG_DEBUG(QString("CommandHandler: CC1=0x%1 CC2=0x%2 cc1Hi=0x%3"
        " [CC2: msgOn=%4 msgOff=%5 alarm=%6 unlockKbd=%7 blinkOn=%8 blinkOff=%9 icUlock=%10]")
        .arg(cc1, 2, 16, QChar('0')).arg(cc2, 2, 16, QChar('0'))
        .arg(cc1Hi, 2, 16, QChar('0'))
        .arg((cc2 & 0x01) ? "Y" : "N")
        .arg((cc2 & 0x02) ? "Y" : "N")
        .arg((cc2 & 0x04) ? "Y" : "N")
        .arg((cc2 & 0x08) ? "Y" : "N")
        .arg((cc2 & 0x10) ? "Y" : "N")
        .arg((cc2 & 0x20) ? "Y" : "N")
        .arg((cc2 & 0x40) ? "Y" : "N"));

    // CC1 top 3 bits determine lock + MDT/field operations
    // All values except 0x00 lock the keyboard
    bool lockKbd = (cc1Hi != 0x00);
    bool resetNonBypassMdt = false;
    bool resetAllMdt = false;
    bool nullNonBypass = false;

    switch (cc1Hi) {
    case 0x00: // No lock, no MDT operations
        break;
    case 0x20: // Lock only
        break;
    case 0x40: // Lock + reset non-bypass MDT
        resetNonBypassMdt = true;
        break;
    case 0x60: // Lock + reset all MDT
        resetAllMdt = true;
        break;
    case 0x80: // Lock + null non-bypass fields
        nullNonBypass = true;
        break;
    case 0xA0: // Lock + reset non-bypass MDT + null non-bypass fields
        resetNonBypassMdt = true;
        nullNonBypass = true;
        break;
    case 0xC0: // Lock + reset non-bypass MDT + null non-bypass MDT fields
        resetNonBypassMdt = true;
        nullNonBypass = true;
        break;
    case 0xE0: // Lock + reset all MDT + null non-bypass fields
        resetAllMdt = true;
        nullNonBypass = true;
        break;
    default:
        break;
    }

    if (nullNonBypass) {
        for (const auto &field : screen->fields()) {
            if (!field.protected_field && !field.bypass) {
                int addr = field.startRow * screen->cols() + field.startCol;
                for (int j = 0; j < field.length; ++j) {
                    int r = (addr + j) / screen->cols();
                    int c = (addr + j) % screen->cols();
                    screen->writeChar(r, c, 0x00);
                }
            }
        }
    }

    if (resetAllMdt) {
        screen->resetAllMDTFlags();
    } else if (resetNonBypassMdt) {
        // Reset MDT only on non-bypass fields
        for (auto &field : screen->mutableFields()) {
            if (!field.bypass) {
                field.modified = false;
            }
        }
    }

    if (lockKbd) {
        m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Locked);
    }

    m_pendingCC2 = cc2;
}

void TN5250CommandHandler::processDeferredCC2(uint8_t cc2) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    // CC2 bit masks per SA21-9247 / lib5250 reference:
    //   0x01=msgOn  0x02=msgOff  0x04=alarm  0x08=unlockKbd
    //   0x10=blinkOn  0x20=blinkOff  0x40=IC_unlock
    LOG_DEBUG(QString("CommandHandler: processDeferredCC2: 0x%1"
        " [msgOn=%2 msgOff=%3 alarm=%4 unlockKbd=%5 blinkOn=%6 blinkOff=%7 icUlock=%8]")
        .arg(cc2, 2, 16, QChar('0'))
        .arg((cc2 & 0x01) ? "Y" : "N")
        .arg((cc2 & 0x02) ? "Y" : "N")
        .arg((cc2 & 0x04) ? "Y" : "N")
        .arg((cc2 & 0x08) ? "Y" : "N")
        .arg((cc2 & 0x10) ? "Y" : "N")
        .arg((cc2 & 0x20) ? "Y" : "N")
        .arg((cc2 & 0x40) ? "Y" : "N"));
    auto *screen = m_displayWidget->screenBuffer();

    // 0x01: Message waiting indicator ON
    if (cc2 & 0x01) {
        m_displayWidget->setMessageWaiting(true);
    }
    // 0x02: Message waiting indicator OFF (only if msgOn not also set)
    if ((cc2 & 0x02) && !(cc2 & 0x01)) {
        m_displayWidget->setMessageWaiting(false);
    }
    // 0x04: Sound alarm (beep)
    if (cc2 & 0x04) {
        QApplication::beep();
    }
    // 0x08: Unlock keyboard
    if (cc2 & 0x08) {
        // Restore cursor visibility after host processing
        screen->setCursorVisible(true);
        m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Unlocked);
        int icRow = m_displayWidget->icRow();
        int icCol = m_displayWidget->icCol();
        // Move cursor to IC address unless CC2 bit 6 (0x40) says leave it
        if (!(cc2 & 0x40)) {
            screen->setCursorPosition(icRow, icCol);
            auto field = screen->getField(icRow, icCol);
            if (field.length <= 0 || field.protected_field || field.bypass) {
                const auto &fields = screen->fields();
                for (const auto &f : fields) {
                    if (f.length > 0 && !f.protected_field && !f.bypass) {
                        screen->setCursorPosition(f.startRow, f.startCol);
                        break;
                    }
                }
            }
        }
        if (m_displayWidget->isVisible())
            m_displayWidget->setFocus();
    }
    // 0x10: Turn on cursor blink (cursor visible and blinking)
    if (cc2 & 0x10) {
        m_displayWidget->setCursorBlinkRate(250);
    }
    // 0x20: Turn off cursor blink (cursor visible but fixed/non-blinking)
    if (cc2 & 0x20) {
        m_displayWidget->setCursorBlinkRate(0);
    }
}

void TN5250CommandHandler::onSohReceived(uint8_t errorRow, uint8_t ckm1,
                                          uint8_t ckm2, uint8_t ckm3) {
    if (!m_displayWidget) {
        return;
    }
    if (errorRow > 0) {
        m_displayWidget->setErrorLineRow(errorRow - 1);
    }
    m_displayWidget->setCmdKeyMask(ckm1, ckm2, ckm3);
    logger::Logger::instance()->debug(
        QString("CommandHandler: SOH received - errorRow=%1 cmdKeyMask=%2,%3,%4")
            .arg(errorRow).arg(ckm1, 2, 16, QChar('0'))
            .arg(ckm2, 2, 16, QChar('0')).arg(ckm3, 2, 16, QChar('0')));
}

void TN5250CommandHandler::onRollRequested(uint8_t topRow, uint8_t botRow,
                                            uint8_t lines, bool up) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    m_displayWidget->screenBuffer()->scrollRegion(topRow, botRow, lines, up);
    logger::Logger::instance()->debug(
        QString("CommandHandler: Roll %1 rows=%2-%3 lines=%4")
            .arg(up ? "up" : "down").arg(topRow).arg(botRow).arg(lines));
}

void TN5250CommandHandler::onWriteErrorCode(const QByteArray &errorData) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    auto *screen = m_displayWidget->screenBuffer();
    int errRow = m_displayWidget->errorLineRow();
    if (errRow < 0) errRow = screen->rows() - 1;

    QVector<ui::widgets::ScreenCell> savedLine;
    for (int c = 0; c < screen->cols(); ++c) {
        savedLine.append(screen->cell(errRow, c));
    }
    m_displayWidget->setSavedErrorLine(savedLine);

    for (int c = 0; c < screen->cols(); ++c) {
        screen->writeChar(errRow, c, 0x40);
    }

    int col = 0;
    ui::widgets::CellAttributes errAttr;
    errAttr.color = 12;
    for (int i = 0; i < errorData.size();) {
        uint8_t byte = static_cast<uint8_t>(errorData[i]);
        if (byte == 0x13 && i + 2 < errorData.size()) {
            int icRow = static_cast<uint8_t>(errorData[i + 1]) - 1;
            int icCol = static_cast<uint8_t>(errorData[i + 2]) - 1;
            if (icRow >= 0 && icRow < screen->rows() && icCol >= 0 && icCol < screen->cols()) {
                screen->setCursorPosition(icRow, icCol);
            }
            i += 3;
            continue;
        }
        if (byte >= 0x40 && col < screen->cols()) {
            screen->writeChar(errRow, col, byte, errAttr);
            col++;
        } else if (byte >= 0x20 && byte <= 0x3F) {
            screen->writeChar(errRow, col, 0x40, errAttr);
            col++;
        }
        i++;
    }

    m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::ErrorLocked);
    m_displayWidget->setCursorBlinkRate(250);
    logger::Logger::instance()->debug("CommandHandler: Write Error Code received");
}

void TN5250CommandHandler::onSaveScreenRequested(ui::widgets::ScreenBuffer::SavedState &savedScreen) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    savedScreen = m_displayWidget->screenBuffer()->saveState();
    logger::Logger::instance()->debug("CommandHandler: Screen saved");
}

void TN5250CommandHandler::onClearScreenAlternateRequested() {
    if (!m_displayWidget) {
        return;
    }
    m_displayWidget->setScreenSize(27, 132);
    if (m_displayWidget->screenBuffer()) {
        m_displayWidget->screenBuffer()->clear();
    }
    m_displayWidget->setCursorBlinkRate(0);
    logger::Logger::instance()->debug("CommandHandler: Clear Unit Alternate (27x132)");
}

void TN5250CommandHandler::onClearFormatTableRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    m_displayWidget->screenBuffer()->clearFields();
    m_displayWidget->setCursorBlinkRate(0);
    logger::Logger::instance()->debug("CommandHandler: Format table cleared");
}

QByteArray TN5250CommandHandler::buildFieldResponse(uint8_t aidByte) {
    QByteArray response;

    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        response.append(static_cast<char>(0));
        response.append(static_cast<char>(0));
        response.append(static_cast<char>(aidByte));
        return response;
    }

    auto *screen = m_displayWidget->screenBuffer();
    QPoint cursor = screen->cursorPosition();
    // 5250 response format: row (1-based), col (1-based), AID byte
    response.append(static_cast<char>(cursor.y() + 1));
    response.append(static_cast<char>(cursor.x() + 1));
    response.append(static_cast<char>(aidByte));

    // SBA address points to the field data start position
    QVector<ui::widgets::ScreenBuffer::Field> modFields = screen->getModifiedFields();
    LOG_DEBUG(QString("CommandHandler: buildFieldResponse: aid=0x%1 cursor=(%2,%3) modifiedFields=%4")
        .arg(aidByte, 2, 16, QChar('0')).arg(cursor.y() + 1).arg(cursor.x() + 1)
        .arg(modFields.size()));
    for (const auto &field : modFields) {
        response.append(static_cast<char>(0x11));
        response.append(static_cast<char>(field.startRow + 1));
        response.append(static_cast<char>(field.startCol + 1));
        // Get field data, strip trailing nulls, convert embedded nulls to blanks
        QByteArray fieldData = screen->getFieldData(field);
        while (!fieldData.isEmpty() && fieldData.back() == '\0') {
            fieldData.chop(1);
        }
        for (int k = 0; k < fieldData.size(); ++k) {
            if (fieldData[k] == '\0') {
                fieldData[k] = static_cast<char>(0x40);
            }
        }
        response.append(fieldData);
    }

    return response;
}

void TN5250CommandHandler::onInviteReceived() {
    if (!m_displayWidget) return;
    // Invite unlocks the keyboard for user input
    m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Unlocked);
    m_displayWidget->setCursorBlinkRate(250);
    if (m_displayWidget->isVisible())
        m_displayWidget->setFocus();
    LOG_DEBUG("CommandHandler: Invite received - keyboard unlocked");
}

void TN5250CommandHandler::onCancelInviteReceived() {
    if (!m_displayWidget) return;
    // Lock keyboard and echo Cancel Invite back to host
    m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Locked);
    if (m_sendGDS) {
        m_sendGDS(0x00, tn5250::protocol::GDS_OPCODE_CANCEL_INVITE, QByteArray());
    }
    LOG_DEBUG("CommandHandler: Cancel Invite received - keyboard locked, echoed back");
}

void TN5250CommandHandler::onMessageLightOn() {
    if (!m_displayWidget) return;
    m_displayWidget->setMessageWaiting(true);
    LOG_DEBUG("CommandHandler: Message light ON");
}

void TN5250CommandHandler::onMessageLightOff() {
    if (!m_displayWidget) return;
    m_displayWidget->setMessageWaiting(false);
    LOG_DEBUG("CommandHandler: Message light OFF");
}

void TN5250CommandHandler::sendNegResponse(uint8_t category, uint8_t modifier,
                                            uint8_t uByte1, uint8_t uByte2) {
    if (!m_sendGDS) return;
    QByteArray payload;
    payload.append(static_cast<char>(category));
    payload.append(static_cast<char>(modifier));
    payload.append(static_cast<char>(uByte1));
    payload.append(static_cast<char>(uByte2));
    m_sendGDS(tn5250::protocol::GDS_FLAG_ERR, 0x00, payload);
    LOG_DEBUG(QString("CommandHandler: Sent negative response: cat=0x%1 mod=0x%2")
        .arg(category, 2, 16, QChar('0')).arg(modifier, 2, 16, QChar('0')));
}

void TN5250CommandHandler::onReadScreenRequested(bool includeAttributes) {
    if (!m_sendToHost) return;
    QByteArray response = buildReadScreenResponse(includeAttributes);
    m_sendToHost(response);
    LOG_DEBUG(QString("CommandHandler: Read Screen response sent (%1 bytes, attrs=%2)")
        .arg(response.size()).arg(includeAttributes));
}

QByteArray TN5250CommandHandler::buildReadScreenResponse(bool includeAttributes) {
    QByteArray response;
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) return response;

    auto *screen = m_displayWidget->screenBuffer();
    int rows = screen->rows();
    int cols = screen->cols();

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const auto &cell = screen->cell(r, c);
            if (includeAttributes && cell.attributes.protected_field && cell.attributes.nonDisplay) {
                // This is a field attribute position - emit the attribute byte
                response.append(static_cast<char>(0x20 | (cell.attributes.color & 0x0F)));
            } else {
                uint8_t ch = cell.character;
                if (ch == 0x00) ch = 0x40; // Null → space
                response.append(static_cast<char>(ch));
            }
        }
    }
    return response;
}

void TN5250CommandHandler::onWriteStructuredFieldReceived(const QByteArray &data) {
    // Check for Query (5250) - class 0xD9, type 0x70
    if (data.size() >= 4) {
        uint8_t clazz = static_cast<uint8_t>(data[2]);
        uint8_t type = static_cast<uint8_t>(data[3]);
        if (clazz == 0xD9 && type == 0x70) {
            LOG_DEBUG("CommandHandler: Query (0xD9/0x70) received, sending query response");
            if (m_sendGDS) {
                // Query response uses GDS opcode 0x00 (NOP), not PUT_GET.
                // Payload = cursor(0,0) + WSF AID (0x88) + structured field data.
                m_sendGDS(0x00, 0x00, buildQueryResponse());
            }
            return;
        }
    }
    LOG_DEBUG(QString("CommandHandler: Unhandled Write Structured Field (%1 bytes)")
        .arg(data.size()));
}

QByteArray TN5250CommandHandler::buildQueryResponse() {
    // Build 5250 Query Response per SA21-9247-6 section 15.26.1.
    // Total GDS payload is 64 bytes:
    //   cursor_row(1) + cursor_col(1) + WSF_AID(1) + SF_data(61)
    // Sent with GDS opcode 0x00 (NOP).
    QByteArray resp;
    resp.resize(64, '\0');

    // Inbound WSF header (3 bytes)
    resp[0] = 0x00;                           // Cursor row
    resp[1] = 0x00;                           // Cursor col
    resp[2] = static_cast<char>(0x88);        // Inbound WSF AID

    // Structured field data (starts at offset 3, 61 bytes)
    resp[3]  = 0x00;                          // SF length high
    resp[4]  = 0x40;                          // SF length low (0x0040 = 64, covers offset 3..66 conceptually)
    resp[5]  = static_cast<char>(0xD9);       // Class: 5250 Terminal
    resp[6]  = 0x70;                          // Type: Query Response
    resp[7]  = static_cast<char>(0x80);       // Flag: response to query

    // Controller hardware class (1 byte)
    resp[8]  = 0x06;                          // Remote controller

    // Controller code level (3 bytes)
    resp[9]  = 0x00;
    resp[10] = 0x01;
    resp[11] = 0x01;

    // Reserved (20 bytes: offsets 12-31) — already zeroed by resize

    // Machine type qualifier
    resp[30] = 0x01;

    // Device type (7 EBCDIC bytes: "5251011")
    resp[31] = static_cast<char>(0xF5);       // '5'
    resp[32] = static_cast<char>(0xF2);       // '2'
    resp[33] = static_cast<char>(0xF5);       // '5'
    resp[34] = static_cast<char>(0xF1);       // '1'
    resp[35] = static_cast<char>(0xF0);       // '0'
    resp[36] = static_cast<char>(0xF1);       // '1'
    resp[37] = static_cast<char>(0xF1);       // '1'

    // Keyboard ID
    resp[38] = 0x02;                          // Standard keyboard

    // Extended keyboard ID + reserved (offsets 39-41) — zeroed

    // Display serial number (2 bytes)
    resp[42] = 0x24;
    resp[43] = 0x24;

    // Maximum number of input fields (2 bytes)
    resp[44] = 0x00;
    resp[45] = 0x01;

    // Device capabilities + reserved (offsets 45-49) — zeroed

    // Workstation capabilities (2 bytes)
    resp[50] = 0x01;                          // Read screen, extended attributes
    resp[51] = 0x11;                          // GUI-like characters, SBA on read

    // Reserved (offsets 52-53) — zeroed

    // Additional capability bytes (2 bytes)
    resp[54] = 0x07;                          // DBCS, save/restore screen
    resp[55] = 0x08;                          // Max transmission size indicator

    // Remaining bytes (offsets 56-63) — zeroed by resize

    return resp;
}

} // namespace ui::rendering
