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

#include <QByteArray>
#include <QObject>
#include <cstdint>

class QKeyEvent;

namespace core {

// TN5250 keyboard action types
enum class KeyboardAction {
    NormalKey,  // Normal character input
    PFKey,      // Program Function key (F1-F24)
    Enter,      // Enter key
    Tab,        // Tab key
    BackTab,    // Shift+Tab
    Backspace,  // Backspace
    Delete,     // Delete
    Insert,     // Insert
    Home,       // Home
    End,        // End
    PageUp,     // Page Up
    PageDown,   // Page Down
    ArrowUp,    // Up arrow
    ArrowDown,  // Down arrow
    ArrowLeft,  // Left arrow
    ArrowRight, // Right arrow
    FieldExit,  // Field exit (usually Tab or Enter)
    FieldPlus,  // Field + (cursor to next field)
    FieldMinus, // Field - (cursor to previous field)
    Clear,      // Clear key
    Reset,      // Reset key
    SysReq,     // System Request
    Attn,       // Attention
    RollUp,     // Roll Up
    RollDown,   // Roll Down
    Print       // Print
};

// Keyboard encoder for TN5250 protocol
class KeyboardEncoder : public QObject {
    Q_OBJECT

  public:
    explicit KeyboardEncoder(QObject *parent = nullptr);

    // Encode a key event to TN5250 format
    QByteArray encodeKeyEvent(QKeyEvent *event, bool shiftPressed = false, bool ctrlPressed = false, bool altPressed = false);

    // Encode a PF key (F1-F24)
    QByteArray encodePFKey(int pfNumber);

    // Encode a special key action
    QByteArray encodeAction(KeyboardAction action);

    // Encode a normal character
    QByteArray encodeCharacter(QChar ch);

    // Check if key is a PF key
    static bool isPFKey(int key);

    // Get PF key number from Qt key
    static int getPFKeyNumber(int key);

  signals:
    void keyEncoded(const QByteArray &data);

  private:
    // 5250 AID (Attention ID) codes — per SA21-9247-6 page 2-2
    static constexpr uint8_t AID_ENTER = 0xF1;    // Enter/Rec Adv
    static constexpr uint8_t AID_PF1 = 0x31;      // Command Function 1
    static constexpr uint8_t AID_PF2 = 0x32;      // Command Function 2
    static constexpr uint8_t AID_PF3 = 0x33;      // Command Function 3
    static constexpr uint8_t AID_PF4 = 0x34;      // Command Function 4
    static constexpr uint8_t AID_PF5 = 0x35;      // Command Function 5
    static constexpr uint8_t AID_PF6 = 0x36;      // Command Function 6
    static constexpr uint8_t AID_PF7 = 0x37;      // Command Function 7
    static constexpr uint8_t AID_PF8 = 0x38;      // Command Function 8
    static constexpr uint8_t AID_PF9 = 0x39;      // Command Function 9
    static constexpr uint8_t AID_PF10 = 0x3A;     // Command Function 10
    static constexpr uint8_t AID_PF11 = 0x3B;     // Command Function 11
    static constexpr uint8_t AID_PF12 = 0x3C;     // Command Function 12
    static constexpr uint8_t AID_PF13 = 0xB1;     // Command Function 13
    static constexpr uint8_t AID_PF14 = 0xB2;     // Command Function 14
    static constexpr uint8_t AID_PF15 = 0xB3;     // Command Function 15
    static constexpr uint8_t AID_PF16 = 0xB4;     // Command Function 16
    static constexpr uint8_t AID_PF17 = 0xB5;     // Command Function 17
    static constexpr uint8_t AID_PF18 = 0xB6;     // Command Function 18
    static constexpr uint8_t AID_PF19 = 0xB7;     // Command Function 19
    static constexpr uint8_t AID_PF20 = 0xB8;     // Command Function 20
    static constexpr uint8_t AID_PF21 = 0xB9;     // Command Function 21
    static constexpr uint8_t AID_PF22 = 0xBA;     // Command Function 22
    static constexpr uint8_t AID_PF23 = 0xBB;     // Command Function 23
    static constexpr uint8_t AID_PF24 = 0xBC;     // Command Function 24

    static constexpr uint8_t AID_CLEAR = 0xBD;
    static constexpr uint8_t AID_HELP = 0xF3;
    static constexpr uint8_t AID_ROLLDOWN = 0xF4;  // Roll Down / Page Up
    static constexpr uint8_t AID_ROLLUP = 0xF5;    // Roll Up / Page Down
    static constexpr uint8_t AID_PRINT = 0xF6;
    static constexpr uint8_t AID_RECBS = 0xF8;     // Record Backspace
    static constexpr uint8_t AID_AUTOENTER = 0x3F;  // Auto Enter (Selector Light Pen)

    // Attn and SysReq are Telnet-level signals, not 5250 AID codes.
    // These placeholders keep the existing routing working until
    // proper Telnet-level handling is implemented.
    static constexpr uint8_t AID_ATTN = 0x70;
    static constexpr uint8_t AID_SYSREQ = 0x71;

    uint8_t getAIDForPF(int pfNumber) const;
    uint8_t getAIDForAction(KeyboardAction action) const;
};

} // namespace core
