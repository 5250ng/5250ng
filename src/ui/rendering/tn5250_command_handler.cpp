#include "tn5250_command_handler.h"
#include "logger/logger.h"
#include "network/tn5250/protocol_constants.h"
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

void TN5250CommandHandler::connectDecoder(tn5250::client::Decoder *parser) {
    if (!parser) return;
    connect(parser, &tn5250::client::Decoder::commandReceived,
            this, &TN5250CommandHandler::handleTN5250Command);
    connect(parser, &tn5250::client::Decoder::structuredFieldReceived,
            this, &TN5250CommandHandler::handleStructuredField);
    connect(parser, &tn5250::client::Decoder::rawScreenDataReceived,
            this, &TN5250CommandHandler::handleRawScreenData);
    connect(parser, &tn5250::client::Decoder::clearScreenRequested,
            this, &TN5250CommandHandler::onClearScreenRequested);
    connect(parser, &tn5250::client::Decoder::keyboardUnlockRequested,
            this, &TN5250CommandHandler::onKeyboardUnlockRequested);
    connect(parser, &tn5250::client::Decoder::controlCharactersReceived,
            this, &TN5250CommandHandler::onControlCharactersReceived);
    connect(parser, &tn5250::client::Decoder::sohReceived,
            this, &TN5250CommandHandler::onSohReceived);
    connect(parser, &tn5250::client::Decoder::rollRequested,
            this, &TN5250CommandHandler::onRollRequested);
    connect(parser, &tn5250::client::Decoder::writeErrorCodeRequested,
            this, &TN5250CommandHandler::onWriteErrorCode);
    connect(parser, &tn5250::client::Decoder::clearScreenAlternateRequested,
            this, &TN5250CommandHandler::onClearScreenAlternateRequested);
    connect(parser, &tn5250::client::Decoder::clearFormatTableRequested,
            this, &TN5250CommandHandler::onClearFormatTableRequested);
    connect(parser, &tn5250::client::Decoder::inviteReceived,
            this, &TN5250CommandHandler::onInviteReceived);
    connect(parser, &tn5250::client::Decoder::cancelInviteReceived,
            this, &TN5250CommandHandler::onCancelInviteReceived);
    connect(parser, &tn5250::client::Decoder::messageLightOn,
            this, &TN5250CommandHandler::onMessageLightOn);
    connect(parser, &tn5250::client::Decoder::messageLightOff,
            this, &TN5250CommandHandler::onMessageLightOff);
    connect(parser, &tn5250::client::Decoder::readScreenRequested,
            this, &TN5250CommandHandler::onReadScreenRequested);
    connect(parser, &tn5250::client::Decoder::writeStructuredFieldReceived,
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

    // Process deferred CC2 now that display data has been rendered
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

    uint8_t combo = cc1 & 0x07;
    LOG_DEBUG(QString("CommandHandler: CC1=0x%1 CC2=0x%2 combo=%3"
        " [clearInput=%4 resetMDT=%5 lockKbd=%6 clearFmtTbl=%7]"
        " [nullKbd=%8 blinkOff=%9 unlockKbd=%10 beep=%11 msgOff=%12 msgOn=%13]")
        .arg(cc1, 2, 16, QChar('0')).arg(cc2, 2, 16, QChar('0')).arg(combo)
        .arg((combo >= 5) ? "Y" : "N")
        .arg((combo & 0x04) ? "Y" : "N")
        .arg((combo & 0x01) ? "Y" : "N")
        .arg((combo == 0x07) ? "Y" : "N")
        .arg((cc2 & 0x02) ? "Y" : "N")
        .arg((cc2 & 0x04) ? "Y" : "N")
        .arg((cc2 & 0x08) ? "Y" : "N")
        .arg((cc2 & 0x20) ? "Y" : "N")
        .arg((cc2 & 0x40) ? "Y" : "N")
        .arg((cc2 & 0x80) ? "Y" : "N"));

    if (combo == 0x05 || combo == 0x06 || combo == 0x07) {
        for (const auto &field : screen->fields()) {
            if (!field.protected_field) {
                int addr = field.startRow * screen->cols() + field.startCol;
                for (int j = 0; j < field.length; ++j) {
                    int r = (addr + j) / screen->cols();
                    int c = (addr + j) % screen->cols();
                    screen->writeChar(r, c, 0x00);
                }
            }
        }
    }

    if (combo & 0x04) {
        screen->resetAllMDTFlags();
    }

    if (combo & 0x01) {
        m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Locked);
    }

    if (combo == 0x07) {
        screen->clearFields();
    }

    m_pendingCC2 = cc2;
}

void TN5250CommandHandler::processDeferredCC2(uint8_t cc2) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    LOG_DEBUG(QString("CommandHandler: processDeferredCC2: 0x%1"
        " [blinkOff=%2 unlockKbd=%3 beep=%4 msgOff=%5 msgOn=%6]")
        .arg(cc2, 2, 16, QChar('0'))
        .arg((cc2 & 0x04) ? "Y" : "N")
        .arg((cc2 & 0x08) ? "Y" : "N")
        .arg((cc2 & 0x20) ? "Y" : "N")
        .arg((cc2 & 0x40) ? "Y" : "N")
        .arg((cc2 & 0x80) ? "Y" : "N"));
    auto *screen = m_displayWidget->screenBuffer();

    if (cc2 & 0x04) {
        m_displayWidget->setCursorBlinkRate(0);
    }
    if (cc2 & 0x08) {
        m_displayWidget->setCursorBlinkRate(250);
    }
    if (cc2 & 0x10) {
        m_displayWidget->setKeyboardState(ui::widgets::KeyboardState::Unlocked);
        int icRow = m_displayWidget->icRow();
        int icCol = m_displayWidget->icCol();
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
        if (m_displayWidget->isVisible())
            m_displayWidget->setFocus();
    }
    if (cc2 & 0x20) {
        QApplication::beep();
    }
    if (cc2 & 0x40) {
        m_displayWidget->setMessageWaiting(false);
    }
    if (cc2 & 0x80) {
        m_displayWidget->setMessageWaiting(true);
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
                // This is a field attribute position — emit the attribute byte
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
    // Check for Query (5250) — class 0xD9, type 0x70
    if (data.size() >= 4) {
        uint8_t clazz = static_cast<uint8_t>(data[2]);
        uint8_t type = static_cast<uint8_t>(data[3]);
        if (clazz == 0xD9 && type == 0x70) {
            LOG_DEBUG("CommandHandler: Query (0xD9/0x70) received, sending query response");
            if (m_sendToHost) {
                m_sendToHost(buildQueryResponse());
            }
            return;
        }
    }
    LOG_DEBUG(QString("CommandHandler: Unhandled Write Structured Field (%1 bytes)")
        .arg(data.size()));
}

QByteArray TN5250CommandHandler::buildQueryResponse() {
    // Build 5250 Query Response per SA21-9247-6 section 15.26.1
    // Response mimics a 5251-011 device
    QByteArray resp;

    // Structured field header
    resp.append(static_cast<char>(0x00)); // Length high (filled later)
    resp.append(static_cast<char>(0x00)); // Length low (filled later)
    resp.append(static_cast<char>(0xD9)); // Class: 5250 Terminal
    resp.append(static_cast<char>(0x70)); // Type: Query Response
    resp.append(static_cast<char>(0x80)); // Flag: response to query

    // Controller hardware class (4 bytes)
    resp.append(static_cast<char>(0x06)); // 0x06 = remote controller
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));

    // Controller code level (4 bytes — version 1.1.0.0)
    resp.append(static_cast<char>(0x01));
    resp.append(static_cast<char>(0x01));
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));

    // Reserved (16 bytes)
    for (int i = 0; i < 16; ++i)
        resp.append(static_cast<char>(0x00));

    // Device type (7 EBCDIC bytes: "5251011")
    // EBCDIC: 5=0xF5, 2=0xF2, 1=0xF1, 0=0xF0
    resp.append(static_cast<char>(0xF5)); // '5'
    resp.append(static_cast<char>(0xF2)); // '2'
    resp.append(static_cast<char>(0xF5)); // '5'
    resp.append(static_cast<char>(0xF1)); // '1'
    resp.append(static_cast<char>(0xF0)); // '0'
    resp.append(static_cast<char>(0xF1)); // '1'
    resp.append(static_cast<char>(0xF1)); // '1'

    // Device model (3 EBCDIC bytes: "   ")
    resp.append(static_cast<char>(0x40));
    resp.append(static_cast<char>(0x40));
    resp.append(static_cast<char>(0x40));

    // Keyboard ID (1 byte)
    resp.append(static_cast<char>(0x02)); // Standard keyboard

    // Extended keyboard ID (1 byte)
    resp.append(static_cast<char>(0x00));

    // Reserved (1 byte)
    resp.append(static_cast<char>(0x00));

    // Display serial number (4 bytes)
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));

    // Maximum number of input fields (2 bytes)
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x80)); // 128 fields

    // Reserved (3 bytes)
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));

    // Capabilities flags
    // Row 1, Col 1: screen capability
    int screenRows = 24, screenCols = 80;
    if (m_displayWidget && m_displayWidget->screenBuffer()) {
        screenRows = m_displayWidget->screenBuffer()->rows();
        screenCols = m_displayWidget->screenBuffer()->cols();
    }
    resp.append(static_cast<char>(screenRows));  // Max rows
    resp.append(static_cast<char>(screenCols));   // Max cols

    // Workstation capabilities (2 bytes)
    // Bit flags: supports read screen, supports extended attributes
    resp.append(static_cast<char>(0x01)); // Enhanced display
    resp.append(static_cast<char>(0x00));

    // Fill to standard 64-byte response
    while (resp.size() < 64) {
        resp.append(static_cast<char>(0x00));
    }

    // Fix up length field (bytes 0-1, big-endian, includes length itself)
    int len = resp.size();
    resp[0] = static_cast<char>((len >> 8) & 0xFF);
    resp[1] = static_cast<char>(len & 0xFF);

    return resp;
}

} // namespace ui::rendering
