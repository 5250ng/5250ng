#include "tn5250_command_handler.h"
#include "logger/logger.h"
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
    if (m_displayWidget) {
        m_displayWidget->setFocus();
    }
}

void TN5250CommandHandler::onControlCharactersReceived(uint8_t cc1, uint8_t cc2) {
    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return;
    }
    auto *screen = m_displayWidget->screenBuffer();

    uint8_t combo = cc1 & 0x07;

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
    response.append(static_cast<char>(aidByte));

    if (!m_displayWidget || !m_displayWidget->screenBuffer()) {
        return response;
    }

    auto *screen = m_displayWidget->screenBuffer();
    QPoint cursor = screen->cursorPosition();
    response.append(static_cast<char>(cursor.y() + 1));
    response.append(static_cast<char>(cursor.x() + 1));

    QVector<ui::widgets::ScreenBuffer::Field> modFields = screen->getModifiedFields();
    for (const auto &field : modFields) {
        response.append(static_cast<char>(0x11));
        response.append(static_cast<char>(field.startRow + 1));
        response.append(static_cast<char>(field.startCol + 1));
        response.append(screen->getFieldData(field));
    }

    return response;
}

} // namespace ui::rendering
