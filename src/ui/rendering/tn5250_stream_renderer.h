#pragma once

#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QByteArray>

namespace ui::rendering {

/**
 * Renders a raw TN5250 display data stream into a ScreenBuffer.
 *
 * Handles all display orders (SBA, SF, RA, EA, IC, MC, TD, WDSF) and
 * EBCDIC character data.  Extracted from MainWindow::renderTN5250Stream().
 */
class TN5250StreamRenderer {
  public:
    explicit TN5250StreamRenderer(ui::widgets::Q5250ScreenWidget *widget);

    /**
     * Render display orders and EBCDIC data into the screen buffer.
     * @param data Display orders and EBCDIC data bytes (already stripped of GDS/ESC framing).
     */
    void render(const QByteArray &data);

  private:
    ui::widgets::Q5250ScreenWidget *m_widget;
};

} // namespace ui::rendering
