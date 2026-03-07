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
