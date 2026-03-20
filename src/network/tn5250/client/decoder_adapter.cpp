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

#include "decoder_adapter.h"

namespace tn5250::client {

static QByteArray toQt(const std::vector<uint8_t> &v) {
    return QByteArray(reinterpret_cast<const char *>(v.data()), static_cast<int>(v.size()));
}

static std::vector<uint8_t> fromQt(const QByteArray &ba) {
    return std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(ba.constData()),
                                 reinterpret_cast<const uint8_t *>(ba.constData()) + ba.size());
}

DecoderAdapter::DecoderAdapter(QObject *parent) : QObject(parent) {
    auto &cb = m_decoder.callbacks();

    cb.onCommandReceived = [this](TN5250Command cmd, const std::vector<uint8_t> &data) {
        emit commandReceived(cmd, toQt(data));
    };
    cb.onStructuredFieldReceived = [this](StructuredFieldType type, const std::vector<uint8_t> &data) {
        emit structuredFieldReceived(type, toQt(data));
    };
    cb.onRawScreenData = [this](const std::vector<uint8_t> &data) {
        emit rawScreenDataReceived(toQt(data));
    };
    cb.onClearScreen = [this]() { emit clearScreenRequested(); };
    cb.onClearScreenAlternate = [this]() { emit clearScreenAlternateRequested(); };
    cb.onKeyboardUnlock = [this]() { emit keyboardUnlockRequested(); };
    cb.onControlCharacters = [this](uint8_t cc1, uint8_t cc2) {
        emit controlCharactersReceived(cc1, cc2);
    };
    cb.onSoh = [this](uint8_t er, uint8_t c1, uint8_t c2, uint8_t c3) {
        emit sohReceived(er, c1, c2, c3);
    };
    cb.onRoll = [this](uint8_t top, uint8_t bot, uint8_t lines, bool up) {
        emit rollRequested(top, bot, lines, up);
    };
    cb.onWriteErrorCode = [this](const std::vector<uint8_t> &data) {
        emit writeErrorCodeRequested(toQt(data));
    };
    cb.onSaveScreen = [this]() { emit saveScreenRequested(); };
    cb.onClearFormatTable = [this]() { emit clearFormatTableRequested(); };
    cb.onInvite = [this]() { emit inviteReceived(); };
    cb.onCancelInvite = [this]() { emit cancelInviteReceived(); };
    cb.onMessageLightOn = [this]() { emit messageLightOn(); };
    cb.onMessageLightOff = [this]() { emit messageLightOff(); };
    cb.onReadScreen = [this](bool attrs) { emit readScreenRequested(attrs); };
    cb.onWriteStructuredField = [this](const std::vector<uint8_t> &data) {
        emit writeStructuredFieldReceived(toQt(data));
    };
    cb.onParseError = [this](const std::string &err) {
        emit parseError(QString::fromStdString(err));
    };
}

void DecoderAdapter::parseData(const QByteArray &data) {
    m_decoder.parseData(fromQt(data));
}

void DecoderAdapter::reset() {
    m_decoder.reset();
}

} // namespace tn5250::client
