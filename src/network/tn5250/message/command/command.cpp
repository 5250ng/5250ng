#include "command.h"

namespace tn5250::message::command {

uint32_t unmarshalCommand(const std::vector<uint8_t> &buffer, Command &out, std::string *error) {
    if (buffer.size() < 2) {
        if (error) {
            *error = "5250Command: buffer too short for ESC and command code";
        }
        return 0;
    }

    // Verify the escape byte is present
    if (buffer[0] != 0x04) {
        if (error) {
            *error = "5250Command: escape byte is not 0x04";
        }
        return 0;
    }

    const uint8_t cmd = buffer[1];
    switch (cmd) {

    case CommandCode::TN5250_COMMAND_CLEAR_UNIT: {
        CommandCsClearScreen cs;
        uint32_t read = cs.unmarshal(buffer, error);
        if (read == 0)
            return 0;
        out = cs;
        return read;
    }

    case CommandCode::TN5250_COMMAND_WRITE_TO_DISPLAY: {
        CommandWtdWriteToDisplay wtd;
        uint32_t read = wtd.unmarshal(buffer, error);
        if (read == 0)
            return 0;
        out = wtd;
        return read;
    }

    case CommandCode::TN5250_COMMAND_READ_MDT_FIELDS: {
        CommandRmfReadMdtFields rmf;
        uint32_t read = rmf.unmarshal(buffer, error);
        if (read == 0)
            return 0;
        out = rmf;
        return read;
    }

    default:
        // Commands without full structured parsers: consume remaining bytes
        // so the caller can continue parsing subsequent commands.
        // This covers: 0x20 (Clear Unit Alternate), 0x21 (Write Error Code),
        // 0x23 (Roll), 0x42 (Read Input Fields), 0x50 (Clear Format Table),
        // 0x72 (Read Immediate), and any future commands.
        return static_cast<uint32_t>(buffer.size());
    }
}

} // namespace tn5250::message::command