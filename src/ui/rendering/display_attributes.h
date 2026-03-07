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

#include <cstdint>

namespace ui::rendering {

// IBM 3179-2 full color attribute table (SA21-9247-6, Table 2-3)
// Attribute byte format: 001X XXXX (0x20-0x3F), indexed by (attrByte & 0x1F)
//
// Each entry: { colorIndex, reverse, blink, underline, nonDisplay, colSep }
// Color indices use bright variants (8-15):
//   10=Green, 9=Blue, 11=Cyan, 12=Red, 13=Pink, 14=Yellow, 15=White
struct AttrEntry {
    uint8_t color;
    bool reverse;
    bool blink;
    bool underline;
    bool nonDisplay;
    bool colSep;
};

constexpr AttrEntry kAttributeTable[32] = {
    /* 0x20 */ {10, false, false, false, false, false}, // Green
    /* 0x21 */ {10,  true, false, false, false, false}, // Green, reverse
    /* 0x22 */ {15, false, false, false, false, false}, // White
    /* 0x23 */ {15,  true, false, false, false, false}, // White, reverse
    /* 0x24 */ {10, false, false,  true, false, false}, // Green, underline
    /* 0x25 */ {10,  true, false,  true, false, false}, // Green, reverse+underline
    /* 0x26 */ {15, false, false,  true, false, false}, // White, underline
    /* 0x27 */ {10, false, false, false,  true, false}, // Non-display
    /* 0x28 */ {12, false, false, false, false, false}, // Red
    /* 0x29 */ {12,  true, false, false, false, false}, // Red, reverse
    /* 0x2A */ {12, false,  true, false, false, false}, // Red, blink
    /* 0x2B */ {12,  true,  true, false, false, false}, // Red, reverse+blink
    /* 0x2C */ {12, false, false,  true, false, false}, // Red, underline
    /* 0x2D */ {12,  true, false,  true, false, false}, // Red, reverse+underline
    /* 0x2E */ {12, false,  true,  true, false, false}, // Red, blink+underline
    /* 0x2F */ {12, false, false, false,  true, false}, // Non-display
    /* 0x30 */ {11, false, false, false, false,  true}, // Cyan, col-sep
    /* 0x31 */ {11,  true, false, false, false,  true}, // Cyan, reverse, col-sep
    /* 0x32 */ {14, false, false, false, false,  true}, // Yellow, col-sep
    /* 0x33 */ {14,  true, false, false, false,  true}, // Yellow, reverse, col-sep
    /* 0x34 */ {11, false, false,  true, false,  true}, // Cyan, underline, col-sep
    /* 0x35 */ {11,  true, false,  true, false,  true}, // Cyan, reverse+underline, col-sep
    /* 0x36 */ {14, false, false,  true, false,  true}, // Yellow, underline, col-sep
    /* 0x37 */ {14, false, false, false,  true, false}, // Non-display
    /* 0x38 */ {13, false, false, false, false, false}, // Pink
    /* 0x39 */ {13,  true, false, false, false, false}, // Pink, reverse
    /* 0x3A */ { 9, false, false, false, false, false}, // Blue
    /* 0x3B */ { 9,  true, false, false, false, false}, // Blue, reverse
    /* 0x3C */ {13, false, false,  true, false, false}, // Pink, underline
    /* 0x3D */ {13,  true, false,  true, false, false}, // Pink, reverse+underline
    /* 0x3E */ { 9, false, false,  true, false, false}, // Blue, underline
    /* 0x3F */ { 9, false, false, false,  true, false}, // Non-display
};

} // namespace ui::rendering
