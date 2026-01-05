#include "order.h"
#include "utils/hex/hex.h"
#include <string>
#include <vector>

namespace tn5250::message::command::order {

/**
 * Unmarshal a TN5250 order from the provided buffer.
 *
 * @param buffer Input bytes; must start with ESC (0x04) and a order code.
 * @param out    Output variant populated with the specific order instance.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t unmarshalOrder(const std::vector<uint8_t> &buffer, Order &out, std::string *error) {
    if (buffer.size() < 1) {
        if (error) {
            *error = "buffer too short for order code";
        }
        return 0;
    }

    uint32_t read_bytes = 0;

    uint8_t orderByte = buffer[0];
    switch (orderByte) {

    case OrderCode::START_OF_HEADER: {
        OrderSohStartOfHeader o_soh;
        read_bytes += o_soh.unmarshal(buffer, error);
        if (read_bytes == 0) {
            if (error) {
                *error = "error unmarshalling order -> " + *error;
            }
            return 0;
        }
        out = Order(o_soh);
        break;
    }

    case OrderCode::REPEAT_TO_ADDRESS: {
        OrderRaRepeatToAddress o_ra;
        read_bytes += o_ra.unmarshal(buffer, error);
        if (read_bytes == 0) {
            if (error) {
                *error = "error unmarshalling order -> " + *error;
            }
            return 0;
        }
        out = Order(o_ra);
        break;
    }

    case OrderCode::INSERT_CURSOR: {
        OrderIcInsertCursor o_ic;
        read_bytes += o_ic.unmarshal(buffer, error);
        if (read_bytes == 0) {
            if (error) {
                *error = "error unmarshalling order -> " + *error;
            }
            return 0;
        }
        out = Order(o_ic);
        break;
    }

    case OrderCode::SET_BUFFER_ADDRESS: {
        OrderSbaSetBufferAddress o_sba;
        read_bytes += o_sba.unmarshal(buffer, error);
        if (read_bytes == 0) {
            if (error) {
                *error = "error unmarshalling order -> " + *error;
            }
            return 0;
        }
        out = Order(o_sba);
        break;
    }

    case OrderCode::START_FIELD: {
        OrderSfStartField o_sf;
        read_bytes += o_sf.unmarshal(buffer, error);
        if (read_bytes == 0) {
            if (error) {
                *error = "error unmarshalling order -> " + *error;
            }
            return 0;
        }
        out = Order(o_sf);
        break;
    }

    default:
        if (error) {
            *error = "unknown order code 0x" + utils::hex::to_hex_string_padded_2(orderByte);
        }
        return 0;
    }

    return read_bytes;
}

/**
 * Unmarshal all TN5250 orders present in the provided buffer.
 *
 * The function searches for known order codes and, starting at each found code,
 * calls unmarshalOrder to decode a single order and advance. This repeats until
 * the end of the buffer or an error occurs.
 */
std::vector<Order> unmarshalOrders(const std::vector<uint8_t> &buffer, std::string *error) {
    std::vector<Order> orders;
    size_t offset = 0;

    auto isOrderCode = [](uint8_t b) -> bool {
        switch (b) {
        case OrderCode::START_OF_HEADER:
        case OrderCode::REPEAT_TO_ADDRESS:
        case OrderCode::INSERT_CURSOR:
        case OrderCode::SET_BUFFER_ADDRESS:
        case OrderCode::START_FIELD:
            return true;
        default:
            return false;
        }
    };

    while (offset < buffer.size()) {
        // Advance to the next recognized order code
        while (offset < buffer.size() && !isOrderCode(buffer[offset])) {
            offset++;
        }
        if (offset >= buffer.size()) {
            break;
        }

        const std::vector<uint8_t> sub(buffer.begin() + static_cast<std::ptrdiff_t>(offset), buffer.end());
        Order ord;
        uint32_t read = unmarshalOrder(sub, ord, error);
        if (read == 0) {
            // Propagate error up; return empty on failure
            return {};
        }
        orders.push_back(ord);
        offset += read;
    }

    return orders;
}

} // namespace tn5250::message::command::order