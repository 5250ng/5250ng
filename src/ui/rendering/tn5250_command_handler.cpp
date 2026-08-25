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
#include "core/codepage.h"
#include "logger/logger.h"
#include "tn5250/protocol_constants.h"
#include "ui/dialogs/pc_command_confirm_dialog.h"
#include "utils/hex/hex.h"
#include <QApplication>
#include <vector>

namespace ui::rendering {

namespace {

// A Begin Graphics byte is meaningful only where a 5250 display order may
// begin. IBM i precedes Read Status graphics blocks with SBA/SF orders that
// define the MDT field; an 0xFF after ordinary EBCDIC text is character data.
int findGddmStart(const QByteArray &data) {
    int offset = 0;
    while (offset < data.size()) {
        const uint8_t byte = static_cast<uint8_t>(data[offset]);
        if (byte == 0xFF)
            return offset;

        if (byte == 0x11) { // SBA
            if (offset + 2 >= data.size())
                return -1;
            offset += 3;
            continue;
        }

        if (byte == 0x1D) { // SF, including any optional FCW pairs
            if (offset + 4 >= data.size())
                return -1;
            int fieldOffset = offset + 3;
            while (fieldOffset < data.size()
                   && (static_cast<uint8_t>(data[fieldOffset]) & 0xE0) != 0x20) {
                if (fieldOffset + 1 >= data.size())
                    return -1;
                fieldOffset += 2;
            }
            if (fieldOffset + 2 >= data.size())
                return -1;
            offset = fieldOffset + 3;
            continue;
        }

        return -1;
    }
    return -1;
}

uint8_t completionAid(Gddm5292Decoder::Completion completion) {
    switch (completion) {
    case Gddm5292Decoder::Completion::SystemReset:
        return 0x38; // Command-8
    case Gddm5292Decoder::Completion::RecoverableError:
        return 0x39; // Command-9
    case Gddm5292Decoder::Completion::FatalError:
        return 0x3A; // Command-10
    case Gddm5292Decoder::Completion::Success:
        return 0x3C; // Command-12
    case Gddm5292Decoder::Completion::None:
        return 0;
    }
    return 0;
}

} // namespace

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
    connect(parser, &tn5250::client::DecoderAdapter::saveScreenRequested,
            this, &TN5250CommandHandler::onSaveScreenRequested);
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
    connect(parser, &tn5250::client::DecoderAdapter::strpccmdRequested,
            this, &TN5250CommandHandler::onStrpccmdRequested);
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

        QByteArray graphicsData = data;
        if (!m_gddmDecoder.recognizes(data)) {
            const int beginGraphics = findGddmStart(data);
            if (beginGraphics > 0) {
                if (m_renderer)
                    m_renderer->render(data.left(beginGraphics));
                graphicsData = data.mid(beginGraphics);
            }
        }

        const Gddm5292Decoder::Result graphics = m_gddmDecoder.process(graphicsData);
        if (graphics.handled) {
            m_displayWidget->setGddmGraphicsPlane(m_gddmDecoder.graphicsPlane(),
                                                  m_gddmDecoder.displayEnabled());
            bool includeModifiedFields = false;
            auto *screen = m_displayWidget->screenBuffer();
            const int cells = screen->rows() * screen->cols();
            for (const auto &statusWrite : graphics.statusWrites) {
                for (int index = 0; index < statusWrite.data.size(); ++index) {
                    const int address = (statusWrite.offset + index) % cells;
                    screen->writeChar(address / screen->cols(), address % screen->cols(),
                                      static_cast<uint8_t>(statusWrite.data[index]));
                    screen->markFieldModified(address / screen->cols(),
                                              address % screen->cols());
                }
                includeModifiedFields = true;
            }
            if (graphics.error) {
                logger::Logger::instance()->error(
                    QString("CommandHandler: 5292 graphics error: %1")
                        .arg(graphics.errorMessage));
            }
            const uint8_t aid = completionAid(graphics.completion);
            if (aid != 0 && m_sendToHost)
                m_sendToHost(buildFieldResponse(aid, includeModifiedFields));
            if (graphics.screenCopyRequested) {
                // Order C1. The device would print the composite of both
                // planes; 5250ng reports it instead. Deliberately no file is
                // written here, because that would hand a remote host a
                // file-write primitive on the user's machine.
                // Q5250ScreenWidget::exportCompositeScreen() is the
                // user-invoked equivalent.
                logger::Logger::instance()->info(
                    QString("CommandHandler: 5292 Screen Copy requested by host "
                            "(block %1); no file written, emitting request")
                        .arg(m_gddmDecoder.blockCount()));
                emit screenCopyRequested();
            }
            if (!graphics.warning.isEmpty()) {
                logger::Logger::instance()->warning(
                    QString("CommandHandler: 5292 graphics block %1: %2")
                        .arg(m_gddmDecoder.blockCount()).arg(graphics.warning));
            }
            // One greppable line per block, carrying enough to reconstruct what
            // the decoder saw and what it decided.
            logger::Logger::instance()->debug(
                QString("CommandHandler: 5292 block=%1 bytes=%2 mode=%3 display=%4 "
                        "lastOrder=0x%5 aid=0x%6%7")
                    .arg(m_gddmDecoder.blockCount())
                    .arg(data.size())
                    .arg(m_gddmDecoder.graphicsMode())
                    .arg(m_gddmDecoder.displayEnabled())
                    .arg(m_gddmDecoder.lastOrder(), 2, 16, QChar('0'))
                    .arg(aid, 2, 16, QChar('0'))
                    .arg(graphics.pacingSuppressed ? " suppressed=1" : ""));
            if (graphics.error) {
                logger::Logger::instance()->debug(
                    QString("CommandHandler: 5292 raw block: %1")
                        .arg(QString::fromLatin1(m_gddmDecoder.lastBlock().toHex())));
            }
        } else if (m_renderer) {
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
        // The SOH error row is a raw host byte; everything that later indexes
        // the screen with errorLineRow() (Write Error Code save/clear, error
        // reset restore) goes through ScreenBuffer::cell(), which has no
        // release-mode bounds check. Clamp to the last screen row.
        int row = errorRow - 1;
        if (m_displayWidget->screenBuffer()
            && row >= m_displayWidget->screenBuffer()->rows()) {
            row = m_displayWidget->screenBuffer()->rows() - 1;
        }
        m_displayWidget->setErrorLineRow(row);
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
    if (errRow < 0 || errRow >= screen->rows()) errRow = screen->rows() - 1;

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

void TN5250CommandHandler::onSaveScreenRequested() {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    if (!m_sendGDS) {
        logger::Logger::instance()->warning(
            "CommandHandler: Save Screen requested but no sendGDS callback set - keyboard will stay locked");
        return;
    }
    // Per RFC 2877 / IBM 5250: the host's Save Screen operation requires the
    // client to send back a data stream that, when replayed by the host as a
    // Restore Screen payload, reproduces the current screen. Without this
    // response the host never unlocks the keyboard.
    QByteArray payload = buildSaveScreenResponse();
    m_sendGDS(0x00, tn5250::protocol::GDS_OPCODE_SAVE_SCREEN, payload);
    LOG_DEBUG(QString("CommandHandler: Save Screen response sent (%1 bytes)").arg(payload.size()));
}

QByteArray TN5250CommandHandler::buildSaveScreenResponse() {
    QByteArray out;
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return out;
    }
    using namespace tn5250::protocol;
    auto *screen = m_displayWidget->screenBuffer();
    const int rows = screen->rows();
    const int cols = screen->cols();

    // 1) Clear Unit (or Clear Unit Alternate for 27x132 mode).
    out.append(static_cast<char>(ESC));
    out.append(static_cast<char>((rows == 27 && cols == 132) ? CC_CLEAR_UNIT_ALTERNATE
                                                             : CC_CLEAR_UNIT));

    // 2) Clear format table — SF orders below will rebuild it.
    out.append(static_cast<char>(ESC));
    out.append(static_cast<char>(CC_CLEAR_FORMAT_TABLE));

    // 3) Write To Display. CC1=0, CC2=0x08 unlocks the keyboard when the
    //    host later replays this stream via Restore Screen.
    out.append(static_cast<char>(ESC));
    out.append(static_cast<char>(CC_WRITE_TO_DISPLAY));
    out.append(static_cast<char>(0x00));  // CC1
    out.append(static_cast<char>(0x08));  // CC2: unlock keyboard

    // Track which cells belong to field attribute markers so we skip them
    // when dumping cell content (they are recreated by the SF orders).
    QVector<bool> isAttrMarker(rows * cols, false);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const auto &cell = screen->cell(r, c);
            if (cell.attributes.nonDisplay && cell.attributes.protected_field) {
                isAttrMarker[r * cols + c] = true;
            }
        }
    }

    // 4) Emit an SF order for every known field.
    //    Attribute byte sits one cell before the field data (with wrap).
    for (const auto &f : screen->fields()) {
        int attrAddr = f.startRow * cols + f.startCol - 1;
        if (attrAddr < 0) attrAddr = 0;
        const int attrRow = attrAddr / cols;
        const int attrCol = attrAddr % cols;

        out.append(static_cast<char>(ORDER_SBA));
        out.append(static_cast<char>(attrRow + 1));
        out.append(static_cast<char>(attrCol + 1));

        // Reconstruct a reasonable attribute byte from the stored cell.
        // Field attribute bytes are 0x20-0x3F; low nibble carries display bits.
        uint8_t attrByte = 0x20;
        if (attrRow < rows && attrCol < cols) {
            const auto &attrCell = screen->cell(attrRow, attrCol);
            attrByte = 0x20 | (attrCell.attributes.color & 0x0F);
        }

        out.append(static_cast<char>(ORDER_SF));
        out.append(static_cast<char>(f.ffw1));
        out.append(static_cast<char>(f.ffw2));
        out.append(static_cast<char>(attrByte));
        out.append(static_cast<char>((f.length >> 8) & 0xFF));
        out.append(static_cast<char>(f.length & 0xFF));
    }

    // 5) Dump visible cell content as runs of text, one SBA per run.
    //    Skip attribute marker cells (already emitted by SF above).
    for (int r = 0; r < rows; ++r) {
        int c = 0;
        while (c < cols) {
            while (c < cols && isAttrMarker[r * cols + c]) {
                c++;
            }
            if (c >= cols) break;

            const int runStartCol = c;
            QByteArray runData;
            while (c < cols && !isAttrMarker[r * cols + c]) {
                uint8_t ch = screen->cell(r, c).character;
                // Any byte in 0x00-0x3F would be parsed as a display order by
                // the decoder. Nulls and any stray control-range bytes are
                // coerced to EBCDIC space so the round-trip stays well-formed.
                if (ch < 0x40) ch = 0x40;
                runData.append(static_cast<char>(ch));
                c++;
            }
            if (!runData.isEmpty()) {
                out.append(static_cast<char>(ORDER_SBA));
                out.append(static_cast<char>(r + 1));
                out.append(static_cast<char>(runStartCol + 1));
                out.append(runData);
            }
        }
    }

    // 6) Insert Cursor at the current cursor position.
    QPoint cur = screen->cursorPosition();
    int curRow = qBound(0, cur.y(), rows - 1);
    int curCol = qBound(0, cur.x(), cols - 1);
    out.append(static_cast<char>(ORDER_IC));
    out.append(static_cast<char>(curRow + 1));
    out.append(static_cast<char>(curCol + 1));

    return out;
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

QByteArray TN5250CommandHandler::buildFieldResponse(uint8_t aidByte,
                                                    bool includeModifiedFields) {
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
    QVector<ui::widgets::ScreenBuffer::Field> modFields;
    if (includeModifiedFields)
        modFields = screen->getModifiedFields();
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

void TN5250CommandHandler::onStrpccmdRequested(bool noWait,
                                               const QByteArray &commandBytes) {
    // Always send ENTER AID at the end so the host CL program continues
    // regardless of whether we ran the command. Leaving the keyboard locked is
    // worse than refusing the command — the user can still recover.
    auto sendEnterAndReturn = [this]() {
        if (m_sendToHost) {
            // 0xF1 = AID_ENTER per SA21-9247-6 page 2-2; same literal used at
            // the READ_IMMEDIATE site above. Sending ENTER unconditionally on
            // STRPCCMD ensures the host CL program continues even when we
            // refuse or fail to run the command.
            m_sendToHost(buildFieldResponse(0xF1));
        }
    };

    // Apply the session's configured codepage to the raw EBCDIC bytes the
    // decoder pulled off the wire. Map control characters and undefined
    // codepage entries to a space so they do not break tokenisation
    // downstream.
    const core::CodePage cp(m_codePageId);
    QString command;
    command.reserve(commandBytes.size());
    for (auto rawByte : commandBytes) {
        QChar c = cp.toUnicode(static_cast<uint8_t>(rawByte));
        if (c.isNull() || c.category() == QChar::Other_Control) {
            c = QChar(' ');
        }
        command.append(c);
    }
    command = command.trimmed();

    LOG_DEBUG(QString("CommandHandler: STRPCCMD wait=%1 command=%2")
                  .arg(noWait ? "no" : "yes")
                  .arg(command));

    if (command.isEmpty()) {
        LOG_DEBUG("CommandHandler: STRPCCMD with empty command after trim; sending ENTER and dropping");
        sendEnterAndReturn();
        return;
    }

    // Dispatch on the four STRPCCMD policies. The host always gets ENTER (via
    // sendEnterAndReturn at function exit) regardless of which branch runs.
    switch (m_pcCommandPolicy) {
    case session::PcCommandPolicy::Deny:
        LOG_DEBUG("CommandHandler: STRPCCMD denied silently by policy");
        break;
    case session::PcCommandPolicy::DenyAndAlert:
        LOG_DEBUG("CommandHandler: STRPCCMD denied by policy; alerting user");
        ui::dialogs::PcCommandConfirmDialog::notifyDenied(
            m_hostname, command, m_dialogParent);
        break;
    case session::PcCommandPolicy::AllowWithPrompt:
    case session::PcCommandPolicy::AllowAlways: {
        if (!m_commandRunner) {
            LOG_DEBUG("CommandHandler: STRPCCMD policy allows but runner not configured; refusing");
            break;
        }
        m_commandRunner->setEnabled(true);
        m_commandRunner->setHostname(m_hostname);
        if (m_pcCommandPolicy == session::PcCommandPolicy::AllowWithPrompt) {
            m_commandRunner->setConfirmCallback(
                [this](const QString &host, const QString &cmd) {
                    return ui::dialogs::PcCommandConfirmDialog::ask(host, cmd, m_dialogParent);
                });
        } else {
            // AllowAlways: clear any previously-installed dialog callback so
            // the runner falls through directly to execute.
            m_commandRunner->setConfirmCallback(nullptr);
        }
        m_commandRunner->run(command,
                             noWait ? core::CommandRunner::Mode::NoWait
                                    : core::CommandRunner::Mode::Wait);
        break;
    }
    }

    sendEnterAndReturn();
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
