#pragma once

#include <cstdint>

namespace tn5250::protocol {

// Escape / framing
constexpr uint8_t ESC = 0x04;
constexpr uint8_t SOH = 0x01;

// Command codes (CC byte following ESC)
constexpr uint8_t CC_CLEAR_UNIT           = 0x40;
constexpr uint8_t CC_CLEAR_UNIT_ALTERNATE = 0x20;
constexpr uint8_t CC_WRITE_TO_DISPLAY     = 0x11;
constexpr uint8_t CC_WRITE_ERROR_CODE     = 0x21;
constexpr uint8_t CC_ROLL                 = 0x23;
constexpr uint8_t CC_READ_INPUT_FIELDS    = 0x42;
constexpr uint8_t CC_CLEAR_FORMAT_TABLE   = 0x50;
constexpr uint8_t CC_READ_MDT_FIELDS      = 0x52;
constexpr uint8_t CC_READ_IMMEDIATE       = 0x72;
constexpr uint8_t CC_WRITE_ERROR_CODE_TO_WINDOW = 0x22;
constexpr uint8_t CC_WRITE_STRUCTURED_FIELD = 0xF3;

// Negative response categories
constexpr uint8_t NEG_REQUEST_ERROR        = 0x10;
constexpr uint8_t NEG_REQUEST_REJECT       = 0x08;
constexpr uint8_t NEG_STATE_ERROR          = 0x20;
constexpr uint8_t NEG_USAGE_ERROR          = 0x40;

// GDS opcodes
constexpr uint8_t GDS_OPCODE_INVITE          = 0x01;
constexpr uint8_t GDS_OPCODE_OUTPUT_ONLY     = 0x02;
constexpr uint8_t GDS_OPCODE_PUT_GET         = 0x03;
constexpr uint8_t GDS_OPCODE_SAVE_SCREEN     = 0x04;
constexpr uint8_t GDS_OPCODE_RESTORE         = 0x05;
constexpr uint8_t GDS_OPCODE_READ_SCREEN     = 0x08;
constexpr uint8_t GDS_OPCODE_CANCEL_INVITE   = 0x0A;
constexpr uint8_t GDS_OPCODE_MSG_LIGHT_ON    = 0x0B;
constexpr uint8_t GDS_OPCODE_MSG_LIGHT_OFF   = 0x0C;

// GDS flags (in flagsHi byte)
constexpr uint8_t GDS_FLAG_ERR               = 0x80;  // Error/negative response
constexpr uint8_t GDS_FLAG_ATN               = 0x40;  // Attention key
constexpr uint8_t GDS_FLAG_SRQ               = 0x04;  // System Request

// GDS record constants
constexpr int     GDS_MIN_RECORD_LEN  = 6;
constexpr int     GDS_HEADER_SIZE     = 10;
constexpr uint8_t GDS_RECORD_TYPE_HI  = 0x12;
constexpr uint8_t GDS_RECORD_TYPE_LO  = 0xA0;
constexpr uint8_t GDS_VAR_HDR_LEN     = 0x04;

// 5250 display orders
constexpr uint8_t ORDER_SBA  = 0x11; // Set Buffer Address
constexpr uint8_t ORDER_SF   = 0x1D; // Start Field
constexpr uint8_t ORDER_RA   = 0x02; // Repeat to Address
constexpr uint8_t ORDER_EA   = 0x03; // Erase to Address
constexpr uint8_t ORDER_IC   = 0x13; // Insert Cursor
constexpr uint8_t ORDER_MC   = 0x14; // Move Cursor
constexpr uint8_t ORDER_TD   = 0x10; // Transparent Data
constexpr uint8_t ORDER_WEA  = 0x12; // Write Extended Attribute
constexpr uint8_t ORDER_WDSF = 0x15; // Write to Display Structured Field (new-style)

// Read Screen command codes (ESC + CC)
constexpr uint8_t CC_READ_SCREEN          = 0x62;
constexpr uint8_t CC_READ_SCREEN_ALT      = 0x66;

// Screen dimension defaults
constexpr int DEFAULT_SCREEN_ROWS = 24;
constexpr int DEFAULT_SCREEN_COLS = 80;

// EBCDIC constants
constexpr uint8_t EBCDIC_SPACE = 0x40;

} // namespace tn5250::protocol
