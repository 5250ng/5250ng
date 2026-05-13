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

#include "core/codepage.h"
#include "core/command_runner.h"
#include "network/tn5250_qt/client/decoder_adapter.h"
#include "session/config.h"
#include "tn5250_stream_renderer.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QApplication>
#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <cstdint>
#include <functional>

namespace ui::rendering {

/**
 * Handles TN5250 commands and signals from the Decoder, updating the
 * screen widget accordingly.  Extracted from MainWindow.
 *
 * Delegates display-order rendering to TN5250StreamRenderer.
 */
class TN5250CommandHandler : public QObject {
    Q_OBJECT

  public:
    using SendToHostFn = std::function<void(const QByteArray &)>;
    using SendGDSFn = std::function<void(uint8_t flagsHi, uint8_t opcode, const QByteArray &payload)>;

    explicit TN5250CommandHandler(QObject *parent = nullptr);

    void setDisplayWidget(ui::widgets::Q5250ScreenWidget *widget);
    void setSendToHostCallback(SendToHostFn fn);
    void setSendGDSCallback(SendGDSFn fn);

    // Connect all decoder signals to this handler
    void connectDecoder(tn5250::client::DecoderAdapter *parser);

    // Access the pending CC2 byte (for deferred processing)
    uint8_t pendingCC2() const { return m_pendingCC2; }
    void clearPendingCC2() { m_pendingCC2 = 0; }

    // Public handlers (called from MainWindow slots)
    void handleTN5250Command(tn5250::client::TN5250Command cmd, const QByteArray &data);
    void handleStructuredField(tn5250::client::StructuredFieldType type, const QByteArray &data);
    void handleRawScreenData(const QByteArray &data);
    void onClearScreenRequested();
    void onKeyboardUnlockRequested();
    void onControlCharactersReceived(uint8_t cc1, uint8_t cc2);
    void processDeferredCC2(uint8_t cc2);
    void onSohReceived(uint8_t errorRow, uint8_t ckm1, uint8_t ckm2, uint8_t ckm3);
    void onRollRequested(uint8_t topRow, uint8_t botRow, uint8_t lines, bool up);
    void onWriteErrorCode(const QByteArray &errorCode);
    void onSaveScreenRequested();
    void onClearScreenAlternateRequested();
    void onClearFormatTableRequested();
    void onInviteReceived();
    void onCancelInviteReceived();
    void onMessageLightOn();
    void onMessageLightOff();
    void onReadScreenRequested(bool includeAttributes);
    void onWriteStructuredFieldReceived(const QByteArray &data);
    // STRPCCMD: host has asked us to run a workstation-side command. Receives
    // the wait flag and raw EBCDIC command bytes consumed by the protocol
    // decoder from the wire (immediately after the 10-byte PCO marker),
    // applies the configured codepage, gates the resulting command through
    // the configured CommandRunner, then unconditionally returns ENTER AID
    // so the host CL program continues.
    void onStrpccmdRequested(bool noWait, const QByteArray &commandBytes);

    void sendNegResponse(uint8_t category, uint8_t modifier, uint8_t uByte1, uint8_t uByte2);

    QByteArray buildFieldResponse(uint8_t aidByte);
    QByteArray buildReadScreenResponse(bool includeAttributes);
    QByteArray buildQueryResponse();
    QByteArray buildSaveScreenResponse();

    // Inject runtime context for STRPCCMD handling. The runner owns the
    // policy/confirm/execute flow; the codepage decodes EBCDIC bytes from the
    // screen buffer; the parent widget is used as the dialog parent.
    void setCommandRunner(core::CommandRunner *runner) { m_commandRunner = runner; }
    void setCodePage(core::CodePage::ID id) { m_codePageId = id; }
    void setHostname(const QString &hostname) { m_hostname = hostname; }
    void setDialogParent(QWidget *parent) { m_dialogParent = parent; }
    // STRPCCMD policy — controls how onStrpccmdRequested reacts to a host
    // attempt: Deny silently, Deny + alert, Allow with prompt, Allow always.
    void setPcCommandPolicy(session::PcCommandPolicy policy) { m_pcCommandPolicy = policy; }

  private:
    ui::widgets::Q5250ScreenWidget *m_displayWidget = nullptr;
    TN5250StreamRenderer *m_renderer = nullptr;
    SendToHostFn m_sendToHost;
    SendGDSFn m_sendGDS;
    uint8_t m_pendingCC2 = 0;
    uint8_t m_readType = 0; // 0x52=READ_MDT, 0x42=READ_INPUT
    core::CommandRunner *m_commandRunner = nullptr;
    core::CodePage::ID m_codePageId = core::CodePage::ID::CP037;
    QString m_hostname;
    QPointer<QWidget> m_dialogParent;
    session::PcCommandPolicy m_pcCommandPolicy = session::PcCommandPolicy::DenyAndAlert;
};

} // namespace ui::rendering
