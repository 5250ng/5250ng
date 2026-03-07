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

#include "command_code.h"
#include "utils/hex/hex.h"
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "./commands/command_cs_clear_screen.h"
#include "./commands/command_rmf_read_mdt_fields.h"
#include "./commands/command_wtd_write_to_display.h"

namespace tn5250::message::command {

using Command = std::variant<CommandCsClearScreen, CommandWtdWriteToDisplay, CommandRmfReadMdtFields>;

/**
 * Unmarshal a TN5250 command from the provided buffer.
 *
 * @param buffer Input bytes; must start with ESC (0x04) and a command code.
 * @param out    Output variant populated with the specific command instance.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t unmarshalCommand(const std::vector<uint8_t> &buffer, Command &out, std::string *error = nullptr);

} // namespace tn5250::message::command