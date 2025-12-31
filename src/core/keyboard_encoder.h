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
  QByteArray encodeKeyEvent(QKeyEvent *event, bool shiftPressed = false,
                            bool ctrlPressed = false, bool altPressed = false);

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
  // TN5250 AID (Attention ID) codes
  static constexpr uint8_t AID_ENTER = 0x7D;
  static constexpr uint8_t AID_PF1 = 0xF1;
  static constexpr uint8_t AID_PF2 = 0xF2;
  static constexpr uint8_t AID_PF3 = 0xF3;
  static constexpr uint8_t AID_PF4 = 0xF4;
  static constexpr uint8_t AID_PF5 = 0xF5;
  static constexpr uint8_t AID_PF6 = 0xF6;
  static constexpr uint8_t AID_PF7 = 0xF7;
  static constexpr uint8_t AID_PF8 = 0xF8;
  static constexpr uint8_t AID_PF9 = 0xF9;
  static constexpr uint8_t AID_PF10 = 0x7A;
  static constexpr uint8_t AID_PF11 = 0x7B;
  static constexpr uint8_t AID_PF12 = 0x7C;
  static constexpr uint8_t AID_PF13 = 0xC1;
  static constexpr uint8_t AID_PF14 = 0xC2;
  static constexpr uint8_t AID_PF15 = 0xC3;
  static constexpr uint8_t AID_PF16 = 0xC4;
  static constexpr uint8_t AID_PF17 = 0xC5;
  static constexpr uint8_t AID_PF18 = 0xC6;
  static constexpr uint8_t AID_PF19 = 0xC7;
  static constexpr uint8_t AID_PF20 = 0xC8;
  static constexpr uint8_t AID_PF21 = 0xC9;
  static constexpr uint8_t AID_PF22 = 0x4A;
  static constexpr uint8_t AID_PF23 = 0x4B;
  static constexpr uint8_t AID_PF24 = 0x4C;

  static constexpr uint8_t AID_TAB = 0x05;
  static constexpr uint8_t AID_BACKTAB = 0xF5; // Sometimes same as PF5
  static constexpr uint8_t AID_CLEAR = 0x6D;
  static constexpr uint8_t AID_ATTN = 0x6C;
  static constexpr uint8_t AID_SYSREQ = 0x6F;

  uint8_t getAIDForPF(int pfNumber) const;
  uint8_t getAIDForAction(KeyboardAction action) const;
};

} // namespace core
