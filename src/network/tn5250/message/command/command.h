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