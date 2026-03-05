#pragma once

#include "network/tn5250/client/decoder.h"
#include "tn5250_stream_renderer.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QApplication>
#include <QByteArray>
#include <QObject>
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
    void connectDecoder(tn5250::client::Decoder *parser);

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
    void onSaveScreenRequested(ui::widgets::ScreenBuffer::SavedState &savedScreen);
    void onClearScreenAlternateRequested();
    void onClearFormatTableRequested();
    void onInviteReceived();
    void onCancelInviteReceived();
    void onMessageLightOn();
    void onMessageLightOff();
    void onReadScreenRequested(bool includeAttributes);
    void onWriteStructuredFieldReceived(const QByteArray &data);

    void sendNegResponse(uint8_t category, uint8_t modifier, uint8_t uByte1, uint8_t uByte2);

    QByteArray buildFieldResponse(uint8_t aidByte);
    QByteArray buildReadScreenResponse(bool includeAttributes);
    QByteArray buildQueryResponse();

  private:
    ui::widgets::Q5250ScreenWidget *m_displayWidget = nullptr;
    TN5250StreamRenderer *m_renderer = nullptr;
    SendToHostFn m_sendToHost;
    SendGDSFn m_sendGDS;
    uint8_t m_pendingCC2 = 0;
    uint8_t m_readType = 0; // 0x52=READ_MDT, 0x42=READ_INPUT
};

} // namespace ui::rendering
