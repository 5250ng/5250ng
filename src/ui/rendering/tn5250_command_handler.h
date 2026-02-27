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

    explicit TN5250CommandHandler(QObject *parent = nullptr);

    void setDisplayWidget(ui::widgets::Q5250ScreenWidget *widget);
    void setSendToHostCallback(SendToHostFn fn);

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

    QByteArray buildFieldResponse(uint8_t aidByte);

  private:
    ui::widgets::Q5250ScreenWidget *m_displayWidget = nullptr;
    TN5250StreamRenderer *m_renderer = nullptr;
    SendToHostFn m_sendToHost;
    uint8_t m_pendingCC2 = 0;
};

} // namespace ui::rendering
