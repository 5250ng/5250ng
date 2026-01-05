#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "./orders/order_ic_insert_cursor.h"
#include "./orders/order_ra_repeat_to_address.h"
#include "./orders/order_sba_set_buffer_address.h"
#include "./orders/order_sf_start_field.h"
#include "./orders/order_soh_start_of_header.h"

namespace tn5250::message::command::order {

using Order = std::variant<OrderSohStartOfHeader, OrderIcInsertCursor, OrderRaRepeatToAddress, OrderSbaSetBufferAddress, OrderSfStartField>;

/**
 * Unmarshal a TN5250 order from the provided buffer.
 *
 * @param buffer Input bytes; must start with ESC (0x04) and a order code.
 * @param out    Output variant populated with the specific order instance.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t unmarshalOrder(const std::vector<uint8_t> &buffer, Order &out, std::string *error = nullptr);

/**
 * Unmarshal all TN5250 orders present in the provided buffer.
 *
 * Scans forward to the next recognized order code, then repeatedly calls
 * unmarshalOrder until the end of the buffer or an error occurs.
 *
 * @param buffer Input bytes that may contain one or more orders back-to-back.
 * @param error  Optional error string; set on failure (returns empty vector).
 * @return A vector of decoded orders (empty on failure).
 */
std::vector<Order> unmarshalOrders(const std::vector<uint8_t> &buffer, std::string *error = nullptr);

} // namespace tn5250::message::command::order