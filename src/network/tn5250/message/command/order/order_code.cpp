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

#include "order_code.h"
#include <iomanip>
#include <sstream>

namespace tn5250::message::command::order {

/**
 * Human-readable description of this order.
 *
 * @return A short string describing the order; includes hex code for unknown orders.
 */
std::string OrderCode::description() const {
    switch (value) {
    case START_OF_HEADER:
        return "Start Of Header";
    case REPEAT_TO_ADDRESS:
        return "Repeat To Address";
    case INSERT_CURSOR:
        return "Insert Cursor";
    case SET_BUFFER_ADDRESS:
        return "Set Buffer Address";
    case START_FIELD:
        return "Start Field";
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
           << value << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message::command::order