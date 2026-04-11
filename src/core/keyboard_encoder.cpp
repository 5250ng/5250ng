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

#include "keyboard_encoder.h"
#include "ebcdic.h"
#include "logger/logger.h"
#include <QKeyEvent>

namespace core {

KeyboardEncoder::KeyboardEncoder(QObject *parent) : QObject(parent) {}

QByteArray KeyboardEncoder::encodeKeyEvent(QKeyEvent *event, bool shiftPressed, bool ctrlPressed, bool altPressed) {
    int key = event->key();
    QString text = event->text();

    // Shift+F1..F12 → F13..F24 (for keyboards without physical F13-F24)
    if (shiftPressed && key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        int pfNumber = (key - Qt::Key_F1) + 13;
        return encodePFKey(pfNumber);
    }

    // Check for PF keys
    if (isPFKey(key)) {
        int pfNumber = getPFKeyNumber(key);
        return encodePFKey(pfNumber);
    }

    // Check for special keys
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return encodeAction(KeyboardAction::Enter);

    case Qt::Key_Tab:
        if (shiftPressed) {
            return encodeAction(KeyboardAction::BackTab);
        }
        return encodeAction(KeyboardAction::Tab);

    case Qt::Key_Backspace:
        return encodeAction(KeyboardAction::Backspace);

    case Qt::Key_Delete:
        return encodeAction(KeyboardAction::Delete);

    case Qt::Key_Insert:
        return encodeAction(KeyboardAction::Insert);

    case Qt::Key_Home:
        return encodeAction(KeyboardAction::Home);

    case Qt::Key_End:
        return encodeAction(KeyboardAction::End);

    case Qt::Key_PageUp:
        return encodeAction(KeyboardAction::RollUp);

    case Qt::Key_PageDown:
        return encodeAction(KeyboardAction::RollDown);

    case Qt::Key_Up:
        return encodeAction(KeyboardAction::ArrowUp);

    case Qt::Key_Down:
        return encodeAction(KeyboardAction::ArrowDown);

    case Qt::Key_Left:
        return encodeAction(KeyboardAction::ArrowLeft);

    case Qt::Key_Right:
        return encodeAction(KeyboardAction::ArrowRight);

    case Qt::Key_Escape:
        if (ctrlPressed) {
            return encodeAction(KeyboardAction::Attn);
        }
        break;

    case Qt::Key_SysReq:
        return encodeAction(KeyboardAction::SysReq);

    default:
        break;
    }

    // Normal character input
    if (!text.isEmpty() && text[0].isPrint()) {
        return encodeCharacter(text[0]);
    }

    // Unknown key
    return QByteArray();
}

QByteArray KeyboardEncoder::encodePFKey(int pfNumber) {
    if (pfNumber < 1 || pfNumber > 24) {
        logger::Logger::instance()->warning(QString("KeyboardEncoder: Invalid PF key number: %1").arg(pfNumber));
        return QByteArray();
    }

    uint8_t aid = getAIDForPF(pfNumber);

    // TN5250 PF key format: AID byte
    QByteArray result;
    result.append(aid);

    emit keyEncoded(result);
    return result;
}

QByteArray KeyboardEncoder::encodeAction(KeyboardAction action) {
    uint8_t aid = getAIDForAction(action);

    if (aid == 0) {
        logger::Logger::instance()->warning("KeyboardEncoder: Unsupported action");
        return QByteArray();
    }

    QByteArray result;
    result.append(aid);

    emit keyEncoded(result);
    return result;
}

QByteArray KeyboardEncoder::encodeCharacter(QChar ch) {
    // Convert character to EBCDIC
    uint8_t ebcdic = EBCDIC::charToEBCDIC(ch);

    QByteArray result;
    result.append(ebcdic);

    emit keyEncoded(result);
    return result;
}

bool KeyboardEncoder::isPFKey(int key) {
    return (key >= Qt::Key_F1 && key <= Qt::Key_F24);
}

int KeyboardEncoder::getPFKeyNumber(int key) {
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return (key - Qt::Key_F1) + 1;
    }
    return 0;
}

uint8_t KeyboardEncoder::getAIDForPF(int pfNumber) const {
    switch (pfNumber) {
    case 1:
        return AID_PF1;
    case 2:
        return AID_PF2;
    case 3:
        return AID_PF3;
    case 4:
        return AID_PF4;
    case 5:
        return AID_PF5;
    case 6:
        return AID_PF6;
    case 7:
        return AID_PF7;
    case 8:
        return AID_PF8;
    case 9:
        return AID_PF9;
    case 10:
        return AID_PF10;
    case 11:
        return AID_PF11;
    case 12:
        return AID_PF12;
    case 13:
        return AID_PF13;
    case 14:
        return AID_PF14;
    case 15:
        return AID_PF15;
    case 16:
        return AID_PF16;
    case 17:
        return AID_PF17;
    case 18:
        return AID_PF18;
    case 19:
        return AID_PF19;
    case 20:
        return AID_PF20;
    case 21:
        return AID_PF21;
    case 22:
        return AID_PF22;
    case 23:
        return AID_PF23;
    case 24:
        return AID_PF24;
    default:
        return 0;
    }
}

uint8_t KeyboardEncoder::getAIDForAction(KeyboardAction action) const {
    // Only return AID codes for keys that should be sent to the host.
    // Tab, Backspace, Delete, Home, End, Insert, arrows, etc. are handled
    // locally in block-mode and should NOT produce AID bytes.
    switch (action) {
    case KeyboardAction::Enter:
        return AID_ENTER;
    case KeyboardAction::Clear:
        return AID_CLEAR;
    case KeyboardAction::Attn:
        return AID_ATTN;
    case KeyboardAction::SysReq:
        return AID_SYSREQ;
    case KeyboardAction::RollUp:
        return AID_ROLLUP;
    case KeyboardAction::RollDown:
        return AID_ROLLDOWN;
    case KeyboardAction::Print:
        return AID_PRINT;
    default:
        // Local-only actions (Tab, BackTab, Backspace, Delete, Insert,
        // Home, End, PageUp, PageDown, arrows) return 0 - handled locally.
        return 0;
    }
}

} // namespace core
