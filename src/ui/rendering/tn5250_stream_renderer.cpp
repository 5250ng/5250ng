#include "tn5250_stream_renderer.h"
#include "display_attributes.h"
#include "logger/logger.h"
#include "network/tn5250/protocol_constants.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"

namespace ui::rendering {

TN5250StreamRenderer::TN5250StreamRenderer(ui::widgets::Q5250ScreenWidget *widget)
    : m_widget(widget) {}

void TN5250StreamRenderer::render(const QByteArray &data) {
    if (!m_widget || !m_widget->screenBuffer()) {
        return;
    }

    LOG_DEBUG(QString("[StreamRenderer] render: %1 bytes of display data").arg(data.size()));

    auto *screen = m_widget->screenBuffer();
    int currentRow = 0;
    int currentCol = 0;
    ui::widgets::CellAttributes currentAttr;
    int i = 0;
    int totalCells = screen->rows() * screen->cols();

    while (i < data.size()) {
        uint8_t byte = static_cast<uint8_t>(data[i]);

        // 5250 display orders are in the 0x00-0x3F range.
        // EBCDIC printable characters are 0x40-0xFF.
        if (byte >= 0x40) {
            // Regular EBCDIC character data — write to screen
            screen->writeChar(currentRow, currentCol, byte, currentAttr);
            currentCol++;
            if (currentCol >= screen->cols()) {
                currentCol = 0;
                currentRow++;
                if (currentRow >= screen->rows()) {
                    currentRow = screen->rows() - 1;
                }
            }
            i++;
            continue;
        }

        // Handle display orders (0x00-0x3F range)
        switch (byte) {
        case 0x11: { // SBA - Set Buffer Address
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            currentRow = static_cast<uint8_t>(data[i + 1]) - 1;
            currentCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (currentRow < 0) currentRow = 0;
            if (currentCol < 0) currentCol = 0;
            if (currentRow >= screen->rows()) currentRow = screen->rows() - 1;
            if (currentCol >= screen->cols()) currentCol = screen->cols() - 1;
            LOG_DEBUG(QString("[StreamRenderer] SBA: row=%1 col=%2").arg(currentRow).arg(currentCol));
            currentAttr = ui::widgets::CellAttributes();
            i += 3;
            break;
        }

        case 0x1D: { // SF - Start Field
            if (i + 4 >= data.size()) {
                i = data.size();
                break;
            }
            uint8_t ffw1 = static_cast<uint8_t>(data[i + 1]);
            uint8_t ffw2 = static_cast<uint8_t>(data[i + 2]);
            int idx = i + 3;

            // Skip optional FCW pairs until attribute byte (bits 7-5 = 001)
            while (idx + 1 < data.size()) {
                uint8_t b = static_cast<uint8_t>(data[idx]);
                if ((b & 0xE0) == 0x20) {
                    break;
                }
                idx += 2;
            }

            if (idx >= data.size()) { i = data.size(); break; }
            uint8_t attrByte = static_cast<uint8_t>(data[idx]);
            idx++;

            if (idx + 1 >= data.size()) { i = data.size(); break; }
            int fieldLen = (static_cast<uint8_t>(data[idx]) << 8) | static_cast<uint8_t>(data[idx + 1]);
            idx += 2;

            // Write BLANK at attribute byte position
            ui::widgets::CellAttributes attrPosAttr;
            attrPosAttr.protected_field = true;
            attrPosAttr.nonDisplay = true;
            screen->writeChar(currentRow, currentCol, 0x40, attrPosAttr);

            // Advance past attribute byte position
            int nextAddr = currentRow * screen->cols() + currentCol + 1;
            int fieldStartRow = nextAddr / screen->cols();
            int fieldStartCol = nextAddr % screen->cols();

            // Register the field
            bool isProtected = (ffw1 & 0x20) != 0;
            LOG_DEBUG(QString("[StreamRenderer] SF: attrPos=(%1,%2) fieldStart=(%3,%4) len=%5"
                " ffw1=0x%6 ffw2=0x%7 prot=%8 bypass=%9 attr=0x%10")
                .arg(currentRow).arg(currentCol).arg(fieldStartRow).arg(fieldStartCol)
                .arg(fieldLen).arg(ffw1, 2, 16, QChar('0')).arg(ffw2, 2, 16, QChar('0'))
                .arg(isProtected).arg((ffw1 & 0x20) != 0)
                .arg(attrByte, 2, 16, QChar('0')));
            screen->setField(fieldStartRow, fieldStartCol, fieldLen, isProtected);
            screen->setFieldFFW(fieldStartRow, fieldStartCol, ffw1, ffw2);

            currentAttr = ui::widgets::CellAttributes();
            currentAttr.protected_field = isProtected;

            uint8_t tableIdx = attrByte & 0x1F;
            const auto &ae = kAttributeTable[tableIdx];
            currentAttr.color = ae.color;
            currentAttr.reverse = ae.reverse;
            currentAttr.blink = ae.blink;
            currentAttr.underline = ae.underline;
            currentAttr.nonDisplay = ae.nonDisplay;
            currentAttr.colSep = ae.colSep;

            // Pre-apply the field's display attributes to all cells in the
            // field extent.  The data stream may not contain explicit data
            // bytes for every position (e.g. empty input fields), so cells
            // left over from a prior Clear Unit would otherwise keep their
            // default attributes (no underline, no color, etc.).
            for (int fi = 0; fi < fieldLen; ++fi) {
                int addr = nextAddr + fi;
                int r = addr / screen->cols();
                int c = addr % screen->cols();
                if (r >= screen->rows()) break;
                auto &sc = screen->cell(r, c);
                sc.attributes.color = currentAttr.color;
                sc.attributes.reverse = currentAttr.reverse;
                sc.attributes.blink = currentAttr.blink;
                sc.attributes.underline = currentAttr.underline;
                sc.attributes.nonDisplay = currentAttr.nonDisplay;
                sc.attributes.colSep = currentAttr.colSep;
                sc.attributes.protected_field = currentAttr.protected_field;
            }

            currentRow = fieldStartRow;
            currentCol = fieldStartCol;
            i = idx;
            break;
        }

        case 0x02: { // RA - Repeat to Address
            if (i + 3 >= data.size()) {
                i = data.size();
                break;
            }
            int targetRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int targetCol = static_cast<uint8_t>(data[i + 2]) - 1;
            uint8_t fillChar = static_cast<uint8_t>(data[i + 3]);
            if (targetRow < 0) targetRow = 0;
            if (targetCol < 0) targetCol = 0;
            LOG_DEBUG(QString("[StreamRenderer] RA: from=(%1,%2) to=(%3,%4) fillChar=0x%5")
                .arg(currentRow).arg(currentCol).arg(targetRow).arg(targetCol)
                .arg(fillChar, 2, 16, QChar('0')));
            i += 4;

            int currentAddr = currentRow * screen->cols() + currentCol;
            int targetAddr = targetRow * screen->cols() + targetCol;

            for (int addr = currentAddr; addr <= targetAddr && addr < totalCells; ++addr) {
                int r = addr / screen->cols();
                int c = addr % screen->cols();
                screen->writeChar(r, c, fillChar, currentAttr);
            }

            int nextAddr = targetAddr + 1;
            if (nextAddr < totalCells) {
                currentRow = nextAddr / screen->cols();
                currentCol = nextAddr % screen->cols();
            } else {
                currentRow = screen->rows() - 1;
                currentCol = screen->cols() - 1;
            }
            break;
        }

        case 0x03: { // EA - Erase to Address
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int targetRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int targetCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (targetRow < 0) targetRow = 0;
            if (targetCol < 0) targetCol = 0;
            LOG_DEBUG(QString("[StreamRenderer] EA: from=(%1,%2) to=(%3,%4)")
                .arg(currentRow).arg(currentCol).arg(targetRow).arg(targetCol));
            i += 3;

            int currentAddr = currentRow * screen->cols() + currentCol;
            int targetAddr = targetRow * screen->cols() + targetCol;

            for (int addr = currentAddr; addr <= targetAddr && addr < totalCells; ++addr) {
                int r = addr / screen->cols();
                int c = addr % screen->cols();
                screen->writeChar(r, c, 0x40, ui::widgets::CellAttributes());
            }

            int nextAddr = targetAddr + 1;
            if (nextAddr < totalCells) {
                currentRow = nextAddr / screen->cols();
                currentCol = nextAddr % screen->cols();
            } else {
                currentRow = screen->rows() - 1;
                currentCol = screen->cols() - 1;
            }
            break;
        }

        case 0x13: { // IC - Insert Cursor
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int icRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int icCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (icRow < 0) icRow = 0;
            if (icCol < 0) icCol = 0;
            LOG_DEBUG(QString("[StreamRenderer] IC: row=%1 col=%2").arg(icRow).arg(icCol));
            screen->setCursorPosition(icRow, icCol);
            if (m_widget) {
                m_widget->setICAddress(icRow, icCol);
            }
            i += 3;
            break;
        }

        case 0x14: { // MC - Move Cursor
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int mcRow = static_cast<uint8_t>(data[i + 1]) - 1;
            int mcCol = static_cast<uint8_t>(data[i + 2]) - 1;
            if (mcRow < 0) mcRow = 0;
            if (mcCol < 0) mcCol = 0;
            LOG_DEBUG(QString("[StreamRenderer] MC: row=%1 col=%2").arg(mcRow).arg(mcCol));
            currentRow = mcRow;
            currentCol = mcCol;
            i += 3;
            break;
        }

        case 0x12: { // WEA - Write Extended Attribute
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            uint8_t weaType = static_cast<uint8_t>(data[i + 1]);
            uint8_t weaValue = static_cast<uint8_t>(data[i + 2]);
            LOG_DEBUG(QString("[StreamRenderer] WEA: type=0x%1 value=0x%2 at (%3,%4)")
                .arg(weaType, 2, 16, QChar('0')).arg(weaValue, 2, 16, QChar('0'))
                .arg(currentRow).arg(currentCol));
            switch (weaType) {
            case 0x01: // Foreground color
                // Color mapping per 5250 spec
                switch (weaValue) {
                case 0x20: currentAttr.color = 2; break;  // Green (default)
                case 0x21: currentAttr.color = 2; break;  // Green
                case 0x22: currentAttr.color = 9; break;  // Blue
                case 0x23: currentAttr.color = 12; break; // Red
                case 0x24: currentAttr.color = 13; break; // Pink/Magenta
                case 0x25: currentAttr.color = 2; break;  // Green
                case 0x26: currentAttr.color = 11; break; // Turquoise/Cyan
                case 0x27: currentAttr.color = 15; break; // White
                case 0x28: // Extended color IDs
                case 0x29: case 0x2A: case 0x2B: case 0x2C:
                case 0x2D: case 0x2E: case 0x2F:
                    currentAttr.color = weaValue & 0x0F; break;
                default:
                    if (weaValue >= 0xF0) currentAttr.color = weaValue & 0x0F;
                    break;
                }
                break;
            case 0x02: // Background color (not directly supported in CellAttributes, apply via reverse)
                break;
            case 0x03: // Character attributes
                currentAttr.underline = (weaValue & 0x04) != 0;
                currentAttr.blink = (weaValue & 0x08) != 0;
                currentAttr.reverse = (weaValue & 0x02) != 0;
                currentAttr.colSep = (weaValue & 0x10) != 0;
                break;
            default:
                break;
            }
            i += 3;
            break;
        }

        case 0x15: { // WDSF - Write to Display Structured Field
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            int wdsfLen = (static_cast<uint8_t>(data[i + 1]) << 8) |
                           static_cast<uint8_t>(data[i + 2]);
            if (wdsfLen < 2) wdsfLen = 2;

            // Parse WDSF minor type if available
            int wdsfStart = i + 3; // Past order + 2-byte length
            int wdsfEnd = i + 1 + wdsfLen;
            if (wdsfEnd > data.size()) wdsfEnd = data.size();

            if (wdsfStart < wdsfEnd) {
                uint8_t wdsfType = static_cast<uint8_t>(data[wdsfStart]);
                LOG_DEBUG(QString("[StreamRenderer] WDSF: len=%1 type=0x%2")
                    .arg(wdsfLen).arg(wdsfType, 2, 16, QChar('0')));

                if (wdsfType == 0x51) {
                    // Create Window (0xD9/0x51)
                    // Parse window depth, width, and border characters
                    int wi = wdsfStart + 1;
                    // Flags byte
                    uint8_t wflags = (wi < wdsfEnd) ? static_cast<uint8_t>(data[wi++]) : 0;
                    Q_UNUSED(wflags);
                    // Reserved
                    if (wi < wdsfEnd) wi++;
                    // Window depth (rows) and width (cols)
                    uint8_t winDepth = (wi < wdsfEnd) ? static_cast<uint8_t>(data[wi++]) : 0;
                    uint8_t winWidth = (wi < wdsfEnd) ? static_cast<uint8_t>(data[wi++]) : 0;

                    LOG_DEBUG(QString("[StreamRenderer] WDSF CreateWindow: depth=%1 width=%2 at (%3,%4)")
                        .arg(winDepth).arg(winWidth).arg(currentRow).arg(currentCol));

                    if (winDepth > 0 && winWidth > 0) {
                        // Draw window borders
                        // Default border chars: TL='.', TR='.', BL=':', BR=':', H='-', V='|'
                        // EBCDIC: '-'=0x60, '|'=0x4F, '.'=0x4B, ':'=0x7A
                        uint8_t borderH = 0x60;  // '-'
                        uint8_t borderV = 0x4F;  // '|'
                        uint8_t borderTL = 0x4B; // '.'
                        uint8_t borderTR = 0x4B; // '.'
                        uint8_t borderBL = 0x7A; // ':'
                        uint8_t borderBR = 0x7A; // ':'

                        // Parse optional border presentation minor structure
                        while (wi + 1 < wdsfEnd) {
                            uint8_t minorLen = static_cast<uint8_t>(data[wi]);
                            uint8_t minorType = static_cast<uint8_t>(data[wi + 1]);
                            if (minorType == 0x01 && minorLen >= 8 && wi + minorLen <= wdsfEnd) {
                                // Border presentation — extract border characters
                                // Skip flags (2 bytes), then: TL, T, TR, L, R, BL, B, BR
                                int bci = wi + 4; // past len, type, 2 flag bytes
                                if (bci + 7 < wi + minorLen) {
                                    borderTL = static_cast<uint8_t>(data[bci]);
                                    borderH  = static_cast<uint8_t>(data[bci + 1]);
                                    borderTR = static_cast<uint8_t>(data[bci + 2]);
                                    borderV  = static_cast<uint8_t>(data[bci + 3]);
                                    // bci+4 = right border (same as left)
                                    borderBL = static_cast<uint8_t>(data[bci + 5]);
                                    // bci+6 = bottom border (same as top)
                                    borderBR = static_cast<uint8_t>(data[bci + 7]);
                                }
                            }
                            wi += minorLen;
                            if (minorLen == 0) break; // prevent infinite loop
                        }

                        // Top row — SBA already set the window start position
                        // Adjust for border: window top-left is (currentRow-1, currentCol-1)
                        int winStartRow = currentRow;
                        int winStartCol = currentCol;

                        ui::widgets::CellAttributes borderAttr;
                        borderAttr.color = 2; // Green borders
                        borderAttr.protected_field = true;

                        // Top border
                        if (winStartRow > 0) {
                            int borderRow = winStartRow - 1;
                            if (winStartCol > 0)
                                screen->writeChar(borderRow, winStartCol - 1, borderTL, borderAttr);
                            for (int c = 0; c < winWidth && winStartCol + c < screen->cols(); ++c)
                                screen->writeChar(borderRow, winStartCol + c, borderH, borderAttr);
                            if (winStartCol + winWidth < screen->cols())
                                screen->writeChar(borderRow, winStartCol + winWidth, borderTR, borderAttr);
                        }

                        // Side borders
                        for (int r = 0; r < winDepth && winStartRow + r < screen->rows(); ++r) {
                            int bRow = winStartRow + r;
                            if (winStartCol > 0)
                                screen->writeChar(bRow, winStartCol - 1, borderV, borderAttr);
                            if (winStartCol + winWidth < screen->cols())
                                screen->writeChar(bRow, winStartCol + winWidth, borderV, borderAttr);
                        }

                        // Bottom border
                        if (winStartRow + winDepth < screen->rows()) {
                            int borderRow = winStartRow + winDepth;
                            if (winStartCol > 0)
                                screen->writeChar(borderRow, winStartCol - 1, borderBL, borderAttr);
                            for (int c = 0; c < winWidth && winStartCol + c < screen->cols(); ++c)
                                screen->writeChar(borderRow, winStartCol + c, borderH, borderAttr);
                            if (winStartCol + winWidth < screen->cols())
                                screen->writeChar(borderRow, winStartCol + winWidth, borderBR, borderAttr);
                        }
                    }
                } else if (wdsfType == 0x5F) {
                    // Remove GUI Window (0xD9/0x5F)
                    LOG_DEBUG("[StreamRenderer] WDSF RemoveGUI");
                    // No action needed — host will redraw underlying screen
                } else {
                    LOG_DEBUG(QString("[StreamRenderer] WDSF: unhandled type 0x%1")
                        .arg(wdsfType, 2, 16, QChar('0')));
                }
            } else {
                LOG_DEBUG(QString("[StreamRenderer] WDSF: len=%1 (no type byte)").arg(wdsfLen));
            }

            i = wdsfEnd;
            break;
        }

        case 0x10: { // TD - Transparent Data
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            uint16_t tdLen = (static_cast<uint16_t>(static_cast<uint8_t>(data[i + 1])) << 8) |
                              static_cast<uint16_t>(static_cast<uint8_t>(data[i + 2]));
            LOG_DEBUG(QString("[StreamRenderer] TD: %1 transparent bytes at (%2,%3)")
                .arg(tdLen).arg(currentRow).arg(currentCol));
            i += 3;
            for (uint16_t t = 0; t < tdLen && i < data.size(); t++) {
                uint8_t tdByte = static_cast<uint8_t>(data[i]);
                screen->writeChar(currentRow, currentCol, tdByte, currentAttr);
                currentCol++;
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        currentRow = screen->rows() - 1;
                    }
                }
                i++;
            }
            break;
        }

        case 0x04: { // ESC - should not appear here (already stripped by Decoder)
            i += 2;
            break;
        }

        default: {
            if (byte == 0x00) {
                // Null character — write to screen (marks unused field positions)
                screen->writeChar(currentRow, currentCol, 0x00, currentAttr);
                currentCol++;
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        currentRow = screen->rows() - 1;
                    }
                }
                i++;
                break;
            }
            if (byte >= 0x20 && byte <= 0x3F) {
                // Attribute indicator byte — occupies a display position (shown as blank)
                ui::widgets::CellAttributes attrPosAttr;
                attrPosAttr.protected_field = true;
                attrPosAttr.nonDisplay = true;
                screen->writeChar(currentRow, currentCol, 0x40, attrPosAttr);
                uint8_t tableIdx = byte & 0x1F;
                const auto &ae = kAttributeTable[tableIdx];
                currentAttr = ui::widgets::CellAttributes();
                currentAttr.color = ae.color;
                currentAttr.reverse = ae.reverse;
                currentAttr.blink = ae.blink;
                currentAttr.underline = ae.underline;
                currentAttr.nonDisplay = ae.nonDisplay;
                currentAttr.colSep = ae.colSep;
                LOG_DEBUG(QString("[StreamRenderer] AttrByte: 0x%1 at (%2,%3) color=%4 rev=%5 ul=%6 nonDisp=%7")
                    .arg(byte, 2, 16, QChar('0')).arg(currentRow).arg(currentCol)
                    .arg(ae.color).arg(ae.reverse).arg(ae.underline).arg(ae.nonDisplay));
                currentCol++;
                if (currentCol >= screen->cols()) {
                    currentCol = 0;
                    currentRow++;
                    if (currentRow >= screen->rows()) {
                        currentRow = screen->rows() - 1;
                    }
                }
                i++;
                break;
            }
            // Truly unrecognized control byte — skip silently
            i++;
            break;
        }
        }
    }

    // Notify screen changed
    m_widget->update();
}

} // namespace ui::rendering
