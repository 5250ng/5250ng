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

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>

#include <tn5250/client/decoder.h>

namespace tn5250::client {

// Qt adapter around the protocol library's callback-based Decoder.
// Bridges std::function callbacks to Qt signals.
class DecoderAdapter : public QObject {
    Q_OBJECT

  public:
    explicit DecoderAdapter(QObject *parent = nullptr);

    void parseData(const QByteArray &data);
    void reset();
    ParserState state() const { return m_decoder.state(); }

  signals:
    void commandReceived(TN5250Command cmd, const QByteArray &data);
    void structuredFieldReceived(StructuredFieldType type, const QByteArray &data);
    void rawScreenDataReceived(const QByteArray &data);
    void clearScreenRequested();
    void clearScreenAlternateRequested();
    void keyboardUnlockRequested();
    void controlCharactersReceived(uint8_t cc1, uint8_t cc2);
    void sohReceived(uint8_t errorRow, uint8_t cmdKeyMask1, uint8_t cmdKeyMask2, uint8_t cmdKeyMask3);
    void rollRequested(uint8_t topRow, uint8_t botRow, uint8_t lines, bool up);
    void writeErrorCodeRequested(const QByteArray &errorCode);
    void saveScreenRequested();
    void clearFormatTableRequested();
    void inviteReceived();
    void cancelInviteReceived();
    void messageLightOn();
    void messageLightOff();
    void readScreenRequested(bool includeAttributes);
    void writeStructuredFieldReceived(const QByteArray &data);
    // Fires when the host's WTD stream contains the STRPCCMD marker. The
    // 10-byte marker has already been consumed by the protocol decoder; the
    // command string itself lives at fixed screen coordinates and must be read
    // off the rendered screen by the consumer (TN5250CommandHandler).
    void strpccmdRequested();
    void parseError(const QString &error);

  private:
    Decoder m_decoder;
};

} // namespace tn5250::client
