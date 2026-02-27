#include "tn5250_stream_renderer.h"
#include "display_attributes.h"
#include "network/tn5250/protocol_constants.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"

namespace ui::rendering {

TN5250StreamRenderer::TN5250StreamRenderer(ui::widgets::Q5250ScreenWidget *widget)
    : m_widget(widget) {}

void TN5250StreamRenderer::render(const QByteArray &data) {
    if (!m_widget || !m_widget->screenBuffer()) {
        return;
    }

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
            currentRow = mcRow;
            currentCol = mcCol;
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
            i += 1 + wdsfLen;
            if (i > data.size()) i = data.size();
            break;
        }

        case 0x10: { // TD - Transparent Data
            if (i + 2 >= data.size()) {
                i = data.size();
                break;
            }
            uint16_t tdLen = (static_cast<uint16_t>(static_cast<uint8_t>(data[i + 1])) << 8) |
                              static_cast<uint16_t>(static_cast<uint8_t>(data[i + 2]));
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
