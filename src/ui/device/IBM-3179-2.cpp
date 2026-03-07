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

#include "IBM-3179-2.h"
#include "logger/logger.h"

using ui::widgets::Q5250ScreenWidget;

namespace ui::device {

IBM3179_2 ::IBM3179_2(QObject *parent) : EmulatedDevice(parent) {}

void IBM3179_2::attachScreen(Q5250ScreenWidget *screen) {
    m_screen = screen;
    if (m_screen) {
        // Ensure screen matches device geometry
        m_screen->setScreenSize(rows(), cols());
        // Device default: top-right cursor on clear
        if (auto *buf = m_screen->screenBuffer()) {
            buf->setCursorPosition(0, cols() - 1);
        }
        m_screen->updateScreen();
    }
}

void IBM3179_2::receiveMessage(const tn5250::message::Message &msg) {
    if (!m_screen) {
        logger::Logger::instance()->warning("IBM3179_2: receiveMessage without attached screen");
        return;
    }

    // Interpret commands in-order
    for (const auto &cmdVariant : msg.commands) {
        std::visit(
            [&](const auto &cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, tn5250::message::command::CommandCsClearScreen>) {
                    handleClearScreen();
                } else if constexpr (std::is_same_v<T, tn5250::message::command::CommandWtdWriteToDisplay>) {
                    handleWriteToDisplay(cmd);
                } else {
                    // Future commands
                }
            },
            cmdVariant
        );
    }
}

void IBM3179_2::handleClearScreen() {
    if (!m_screen)
        return;
    auto *buf = m_screen->screenBuffer();
    if (!buf)
        return;
    buf->clear();
    // IBM3179 default: cursor to top-right
    buf->setCursorPosition(0, cols() - 1);
    m_screen->updateScreen();
}

void IBM3179_2::handleWriteToDisplay(const tn5250::message::command::CommandWtdWriteToDisplay &cmd) {
    if (!m_screen)
        return;
    auto *buf = m_screen->screenBuffer();
    if (!buf)
        return;

    // Handle display orders: SBA, IC, RA, SF, SOH (minimal)
    int curRow = 0;
    int curCol = 0;

    for (const auto &ordVariant : cmd.orders) {
        std::visit(
            [&](const auto &ord) {
                using O = std::decay_t<decltype(ord)>;
                if constexpr (std::is_same_v<O, tn5250::message::command::order::OrderSbaSetBufferAddress>) {
                    curRow = std::max(0, std::min(rows() - 1, static_cast<int>(ord.rowAddress) - 1));
                    curCol = std::max(0, std::min(cols() - 1, static_cast<int>(ord.columnAddress) - 1));
                    // With current parser, SBA may carry trailing data bytes in repeatedCharacter.
                    // Render them sequentially from the current write position.
                    if (!ord.repeatedCharacter.empty()) {
                        for (unsigned char ch : ord.repeatedCharacter) {
                            buf->writeChar(curRow, curCol, static_cast<uint8_t>(ch));
                            ++curCol;
                            if (curCol >= cols()) {
                                curCol = 0;
                                ++curRow;
                                if (curRow >= rows()) {
                                    // Stop at bottom for now
                                    curRow = rows() - 1;
                                    break;
                                }
                            }
                        }
                    }
                } else if constexpr (std::is_same_v<O, tn5250::message::command::order::OrderIcInsertCursor>) {
                    int r = std::max(0, std::min(rows() - 1, static_cast<int>(ord.rowAddress) - 1));
                    int c = std::max(0, std::min(cols() - 1, static_cast<int>(ord.columnAddress) - 1));
                    buf->setCursorPosition(r, c);
                } else if constexpr (std::is_same_v<O, tn5250::message::command::order::OrderRaRepeatToAddress>) {
                    // Repeat filler from current position to target inclusive
                    int endRow = std::max(0, std::min(rows() - 1, static_cast<int>(ord.rowAddress) - 1));
                    int endCol = std::max(0, std::min(cols() - 1, static_cast<int>(ord.columnAddress) - 1));
                    // Simple linear progression (row-major)
                    int r = curRow, c = curCol;
                    const uint8_t ch = ord.repeatedCharacter.empty() ? 0x40 : static_cast<uint8_t>(ord.repeatedCharacter[0]);
                    while (r < rows() && (r < endRow || (r == endRow && c <= endCol))) {
                        buf->writeChar(r, c, ch);
                        ++c;
                        if (c >= cols()) {
                            c = 0;
                            ++r;
                        }
                    }
                    curRow = r;
                    curCol = c;
                } else if constexpr (std::is_same_v<O, tn5250::message::command::order::OrderSfStartField>) {
                    // Apply field attributes and mark field metadata
                    int startRow = curRow;
                    int startCol = curCol;
                    const int length = static_cast<int>(ord.length);
                    if (length > 0) {
                        const bool protectedField = (ord.attributes & 0x80) != 0;
                        buf->setField(startRow, startCol, length, protectedField);
                        const uint8_t color = (ord.attributes & 0x0F);
                        const bool reverse = (ord.attributes & 0x10) != 0;
                        const bool blink = (ord.attributes & 0x20) != 0;
                        const bool underline = (ord.attributes & 0x40) != 0;
                        for (int i = 0; i < length; ++i) {
                            int col = startCol + i;
                            if (col >= cols())
                                break;
                            buf->setColor(startRow, col, color);
                            buf->setReverse(startRow, col, reverse);
                            buf->setBlink(startRow, col, blink);
                            buf->setUnderline(startRow, col, underline);
                        }
                        // If SF provides a repeated character, pre-fill the field
                        if (!ord.repeatedCharacter.empty()) {
                            const uint8_t fill = static_cast<uint8_t>(ord.repeatedCharacter[0]);
                            for (int i = 0; i < length; ++i) {
                                int col = startCol + i;
                                if (col >= cols())
                                    break;
                                buf->writeChar(startRow, col, fill);
                            }
                        }
                    }
                    // Do not advance current write position on SF
                } else if constexpr (std::is_same_v<O, tn5250::message::command::order::OrderSohStartOfHeader>) {
                    // Acknowledge SOH; no direct rendering yet
                } else {
                    // Other orders (SOH, SF, etc.) can be handled here later
                }
            },
            ordVariant
        );
    }

    // Update cursor to last write position if untouched
    buf->setCursorPosition(curRow, curCol);
    m_screen->updateScreen();
}

} // namespace ui::device
