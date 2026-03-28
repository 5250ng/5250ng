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

namespace hostserver {

// IBM i Host Server IDs (bytes 6-7 of the 20-byte header)
enum class ServerID : uint16_t {
    Central   = 0xE000,
    File      = 0xE002,
    Print     = 0xE003,
    Database  = 0xE004,
    DataQueue = 0xE007,
    Command   = 0xE008,
    Signon    = 0xE009,
};

// Default TCP ports for IBM i host servers
namespace ports {
    constexpr uint16_t SERVICE_MAPPER   = 449;
    constexpr uint16_t CENTRAL          = 8470;
    constexpr uint16_t DATABASE         = 8471;
    constexpr uint16_t DATA_QUEUE       = 8472;
    constexpr uint16_t FILE             = 8473;
    constexpr uint16_t PRINT            = 8474;
    constexpr uint16_t COMMAND          = 8475;
    constexpr uint16_t SIGNON           = 8476;

    constexpr uint16_t CENTRAL_TLS      = 9470;
    constexpr uint16_t DATABASE_TLS     = 9471;
    constexpr uint16_t DATA_QUEUE_TLS   = 9472;
    constexpr uint16_t FILE_TLS         = 9473;
    constexpr uint16_t PRINT_TLS        = 9474;
    constexpr uint16_t COMMAND_TLS      = 9475;
    constexpr uint16_t SIGNON_TLS       = 9476;
} // namespace ports

// Common request/reply IDs used across host servers
namespace reqrep {
    constexpr uint16_t EXCHANGE_SEEDS      = 0x7001;
    constexpr uint16_t START_SERVER        = 0x7002;
    constexpr uint16_t SIGNON_EXCHANGE_ATTR = 0x7003;
    constexpr uint16_t SIGNON_INFO         = 0x7004;
} // namespace reqrep

// Code points used in LL/CP variable-length fields
namespace codepoint {
    // Signon code points
    constexpr uint16_t CLIENT_VERSION       = 0x1101;
    constexpr uint16_t DATASTREAM_LEVEL     = 0x1102;
    constexpr uint16_t CLIENT_SEED          = 0x1103;
    constexpr uint16_t USER_ID              = 0x1104;
    constexpr uint16_t ENCRYPTED_PASSWORD   = 0x1105;
    constexpr uint16_t SIGNON_DATE          = 0x1106;
    constexpr uint16_t LAST_SIGNON_DATE     = 0x1107;
    constexpr uint16_t PW_EXPIRATION_DATE   = 0x1108;
    constexpr uint16_t CLIENT_CCSID         = 0x1113;
    constexpr uint16_t SERVER_CCSID         = 0x1114;
    constexpr uint16_t SERVER_SEED          = 0x1103;  // Same CP in reply context
    constexpr uint16_t PASSWORD_LEVEL       = 0x1119;
    constexpr uint16_t RETURN_ERROR_MSGS    = 0x1128;
    constexpr uint16_t PW_EXPIRATION_WARN   = 0x112C;
    constexpr uint16_t AAF_INDICATOR        = 0x112E;
    constexpr uint16_t JOB_NAME             = 0x111F;

    // IFS code points
    constexpr uint16_t IFS_FILENAME         = 0x0002;
    constexpr uint16_t IFS_RENAME_SOURCE    = 0x0003;
    constexpr uint16_t IFS_RENAME_TARGET    = 0x0004;
    constexpr uint16_t IFS_CCSID_LIST       = 0x000A;
    constexpr uint16_t IFS_OA1              = 0x0010;
    constexpr uint16_t IFS_OA2              = 0x000F;
    constexpr uint16_t IFS_FILE_DATA        = 0x0020;
    constexpr uint16_t IFS_DIR_NAME         = 0x0001;
} // namespace codepoint

// Authentication schemes
namespace auth {
    constexpr uint8_t DES_7      = 0x01;  // DES 7-char (QPWDLVL 0-1), 8-byte password
    constexpr uint8_t SHA1       = 0x03;  // SHA-1 (QPWDLVL 2-3), 20-byte password
    constexpr uint8_t SHA512     = 0x07;  // SHA-512/PBKDF2 (QPWDLVL 4+)
} // namespace auth

// IFS request/reply IDs
namespace ifs {
    constexpr uint16_t OPEN_FILE           = 0x0002;
    constexpr uint16_t OPEN_FILE_REPLY     = 0x8002;
    constexpr uint16_t READ_FILE           = 0x0003;
    constexpr uint16_t READ_FILE_REPLY     = 0x8003;
    constexpr uint16_t WRITE_FILE          = 0x0004;
    constexpr uint16_t RETURN_CODE_REPLY   = 0x8004;
    constexpr uint16_t LIST_ATTRS          = 0x000A;
    constexpr uint16_t LIST_ATTRS_REPLY    = 0x8005;
    constexpr uint16_t CLOSE_FILE          = 0x0009;
    constexpr uint16_t DELETE_FILE         = 0x000C;
    constexpr uint16_t CREATE_DIR          = 0x000D;
    constexpr uint16_t DELETE_DIR          = 0x000E;
    constexpr uint16_t RENAME              = 0x000F;
    constexpr uint16_t EXCHANGE_ATTR       = 0x0016;
    constexpr uint16_t EXCHANGE_ATTR_REPLY = 0x8016;
} // namespace ifs

// IFS return codes (16-bit at offset 22)
namespace ifs_rc {
    constexpr uint16_t SUCCESS                  = 0;
    constexpr uint16_t FILE_IN_USE              = 1;
    constexpr uint16_t FILE_NOT_FOUND           = 2;
    constexpr uint16_t PATH_NOT_FOUND           = 3;
    constexpr uint16_t DUPLICATE_DIR_ENTRY      = 4;
    constexpr uint16_t ACCESS_DENIED            = 5;
    constexpr uint16_t INVALID_HANDLE           = 6;
    constexpr uint16_t INVALID_DIR_ENTRY_NAME   = 7;
    constexpr uint16_t DIR_NOT_EMPTY            = 9;
    constexpr uint16_t RESOURCE_LIMIT_EXCEEDED  = 11;
    constexpr uint16_t ACCESS_DENIED_TO_REQUEST = 13;
    constexpr uint16_t INVALID_REQUEST          = 16;
    constexpr uint16_t DATA_STREAM_SYNTAX_ERROR = 17;
    constexpr uint16_t NO_MORE_FILES            = 18;
    constexpr uint16_t NO_MORE_DATA             = 22;
    constexpr uint16_t SHARING_VIOLATION        = 32;
    constexpr uint16_t LOCK_VIOLATION           = 33;
    constexpr uint16_t STALE_HANDLE             = 34;

    // Convert return code to human-readable string
    inline const char *toString(uint16_t rc) {
        switch (rc) {
        case SUCCESS:                   return "Success";
        case FILE_IN_USE:               return "File in use";
        case FILE_NOT_FOUND:            return "File not found";
        case PATH_NOT_FOUND:            return "Path not found";
        case DUPLICATE_DIR_ENTRY:       return "Duplicate directory entry";
        case ACCESS_DENIED:             return "Access denied";
        case INVALID_HANDLE:            return "Invalid handle";
        case INVALID_DIR_ENTRY_NAME:    return "Invalid directory entry name";
        case DIR_NOT_EMPTY:             return "Directory not empty";
        case RESOURCE_LIMIT_EXCEEDED:   return "Resource limit exceeded";
        case ACCESS_DENIED_TO_REQUEST:  return "Access denied to request";
        case INVALID_REQUEST:           return "Invalid request";
        case DATA_STREAM_SYNTAX_ERROR:  return "Data stream syntax error";
        case NO_MORE_FILES:             return "No more files";
        case NO_MORE_DATA:              return "No more data (EOF)";
        case SHARING_VIOLATION:         return "Sharing violation";
        case LOCK_VIOLATION:            return "Lock violation";
        case STALE_HANDLE:              return "Stale handle";
        default:                        return "Unknown error";
        }
    }
} // namespace ifs_rc

// IFS object types (from list attrs reply, offset 54-55)
namespace ifs_objtype {
    constexpr uint16_t FILE       = 1;
    constexpr uint16_t DIRECTORY  = 2;
    constexpr uint16_t SYMLINK    = 3;
    constexpr uint16_t AS400_OBJ  = 4;
    constexpr uint16_t FIFO       = 5;
    constexpr uint16_t CHAR_DEV   = 6;
    constexpr uint16_t BLOCK_DEV  = 7;
    constexpr uint16_t SOCKET_OBJ = 8;
} // namespace ifs_objtype

// IFS fixed attribute bitmask
namespace ifs_attr {
    constexpr uint32_t READ_ONLY  = 0x01;
    constexpr uint32_t HIDDEN     = 0x02;
    constexpr uint32_t SYSTEM     = 0x04;
    constexpr uint32_t DIRECTORY  = 0x10;
    constexpr uint32_t ARCHIVE    = 0x20;
} // namespace ifs_attr

// IFS file access intents (open request)
namespace ifs_access {
    constexpr uint16_t READ_ACCESS   = 0x0001;
    constexpr uint16_t WRITE_ACCESS  = 0x0002;
    constexpr uint16_t PROGRAM_LOAD  = 0x0004;
} // namespace ifs_access

// IFS share modes (open request)
namespace ifs_share {
    constexpr uint16_t DENY_NONE     = 0x0000;
    constexpr uint16_t DENY_READERS  = 0x0001;
    constexpr uint16_t DENY_WRITERS  = 0x0002;
} // namespace ifs_share

// IFS duplicate file options (open request)
namespace ifs_dupopt {
    constexpr uint16_t CREATE_OR_OPEN    = 0x0001;
    constexpr uint16_t CREATE_OR_REPLACE = 0x0002;
    constexpr uint16_t CREATE_FAIL       = 0x0004;
    constexpr uint16_t FAIL_OPEN         = 0x0008;
    constexpr uint16_t FAIL_REPLACE      = 0x0010;
} // namespace ifs_dupopt

} // namespace hostserver
