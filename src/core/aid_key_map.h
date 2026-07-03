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

#include <Qt>
#include <cstdint>

namespace core {

// Reverse map: 5250 AID byte -> the default Qt key chord that produces it.
//
// Script and MCP AID replay (ScriptExecutor::injectAIDKey) synthesizes a
// QKeyEvent from this chord and sends it to Q5250ScreenWidget, whose
// keyPressEvent resolves it through KeyboardMapping's default chord table
// and encodes the mapped action back to an AID byte via KeyboardEncoder
// (dispatchMappedAction). This function must therefore stay the exact
// inverse of those forward mappings — KeyboardMapping::resetToDefaults()
// and KeyboardEncoder's AID codes (SA21-9247-6 page 2-2).
// tests/unit/test_aid_key_map.cpp asserts the round-trip identity for
// every AID byte accepted here.
//
// The roll keys use 5250 window-movement semantics: Roll Down (0xF4) shows
// the previous page and Roll Up (0xF5) the next page, so PC Page Up maps
// to 0xF4 and Page Down to 0xF5 (see the roll-key comment in
// keyboard_encoder.cpp).
//
// Returns false for AID bytes with no chord equivalent (e.g. 0x3F Auto
// Enter, 0xF8 Record Backspace); qtKey/mods are reset to 0/NoModifier.
inline bool aidToQtKey(uint8_t aid, int &qtKey, Qt::KeyboardModifiers &mods) {
    qtKey = 0;
    mods = Qt::NoModifier;
    if (aid == 0xF1) { qtKey = Qt::Key_Return; }                                  // Enter
    else if (aid == 0x70) { qtKey = Qt::Key_Escape; mods = Qt::ControlModifier; } // Attn
    else if (aid == 0x71) { qtKey = Qt::Key_SysReq; }                             // SysReq
    else if (aid >= 0x31 && aid <= 0x3C) { qtKey = Qt::Key_F1 + (aid - 0x31); }   // PF1-PF12
    else if (aid >= 0xB1 && aid <= 0xBC) { qtKey = Qt::Key_F1 + (aid - 0xB1); mods = Qt::ShiftModifier; } // PF13-PF24
    else if (aid == 0xF3) { qtKey = Qt::Key_F1; mods = Qt::ControlModifier; }     // Help
    else if (aid == 0xF4) { qtKey = Qt::Key_PageUp; }                             // Roll Down (previous page)
    else if (aid == 0xF5) { qtKey = Qt::Key_PageDown; }                           // Roll Up (next page)
    else if (aid == 0xF6) { qtKey = Qt::Key_Print; }                              // Print
    else if (aid == 0xBD) { qtKey = Qt::Key_Pause; mods = Qt::ControlModifier; }  // Clear
    else return false;
    return true;
}

} // namespace core
