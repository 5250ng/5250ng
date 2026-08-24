// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "gddm_5292_decoder.h"

#include <QPainter>
#include <QPen>

namespace ui::rendering {

Gddm5292Decoder::Gddm5292Decoder()
    : m_plane(kWidth, kHeight, QImage::Format_ARGB32_Premultiplied) {
    reset(true);
}

void Gddm5292Decoder::reset(bool clearPlane) {
    m_graphicsMode = false;
    m_displayEnabled = false;
    m_suppressPacing = false;
    m_pendingOrder = PendingOrder::None;
    m_pendingData.clear();
    m_palette = {QColor(0, 0, 0), QColor(255, 0, 0), QColor(0, 255, 0),
                 QColor(0, 0, 255), QColor(255, 0, 255), QColor(255, 255, 0),
                 QColor(0, 255, 255), QColor(255, 255, 255)};
    m_colorIndex = 7;
    m_lineWeight = 1;
    if (clearPlane)
        m_plane.fill(Qt::transparent);
}

bool Gddm5292Decoder::recognizes(const QByteArray &data) const {
    return m_graphicsMode
        || (!data.isEmpty() && static_cast<uint8_t>(data.front()) == 0xFF);
}

bool Gddm5292Decoder::isGraphicsData(uint8_t byte) {
    return (byte & 0xC0) == 0x40;
}

int Gddm5292Decoder::decodeCoordinate(uint8_t high, uint8_t low) {
    return ((high & 0x0F) << 6) | (low & 0x3F);
}

bool Gddm5292Decoder::fail(Result &result, int offset, const QString &message) {
    result.error = true;
    result.errorMessage = QString("offset %1: %2").arg(offset).arg(message);
    result.pacingResponse = false;
    m_graphicsMode = false;
    m_pendingOrder = PendingOrder::None;
    m_pendingData.clear();
    return false;
}

bool Gddm5292Decoder::drawPolyline(Result &result) {
    if (m_pendingData.size() < 8 || (m_pendingData.size() % 4) != 0)
        return fail(result, m_pendingData.size(), "polyline requires complete coordinate pairs");

    QPainter painter(&m_plane);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setPen(QPen(m_palette[static_cast<size_t>(m_colorIndex)], m_lineWeight,
                        Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

    auto pointAt = [this](int offset) {
        const int x = decodeCoordinate(static_cast<uint8_t>(m_pendingData[offset]),
                                       static_cast<uint8_t>(m_pendingData[offset + 1]));
        const int y = decodeCoordinate(static_cast<uint8_t>(m_pendingData[offset + 2]),
                                       static_cast<uint8_t>(m_pendingData[offset + 3]));
        return QPoint(x, kHeight - 1 - y);
    };

    QPoint previous = pointAt(0);
    for (int offset = 4; offset < m_pendingData.size(); offset += 4) {
        const QPoint next = pointAt(offset);
        painter.drawLine(previous, next);
        previous = next;
    }
    result.changed = true;
    return true;
}

bool Gddm5292Decoder::completePending(Result &result) {
    bool ok = true;
    if (m_pendingOrder == PendingOrder::Polyline) {
        ok = drawPolyline(result);
    } else if (m_pendingOrder == PendingOrder::ColorTable) {
        if ((m_pendingData.size() % 3) != 0) {
            ok = fail(result, m_pendingData.size(),
                      "color table requires index/red-green/blue triples");
        } else {
            for (int offset = 0; offset < m_pendingData.size(); offset += 3) {
                const int index = static_cast<uint8_t>(m_pendingData[offset]) & 0x07;
                if (index == 0) {
                    ok = fail(result, offset, "color table index zero cannot be changed");
                    break;
                }
                const uint8_t redGreen = static_cast<uint8_t>(m_pendingData[offset + 1]);
                const uint8_t blue = static_cast<uint8_t>(m_pendingData[offset + 2]);
                const int redIntensity = (redGreen >> 3) & 0x07;
                const int greenIntensity = redGreen & 0x07;
                const int blueIntensity = (blue >> 3) & 0x07;
                m_palette[static_cast<size_t>(index)] =
                    QColor(redIntensity * 255 / 7, greenIntensity * 255 / 7,
                           blueIntensity * 255 / 7);
            }
        }
    }
    m_pendingOrder = PendingOrder::None;
    m_pendingData.clear();
    return ok;
}

Gddm5292Decoder::Result Gddm5292Decoder::process(const QByteArray &data) {
    Result result;
    if (!recognizes(data))
        return result;

    result.handled = true;
    ++m_blockCount;
    m_suppressPacing = false;
    int offset = 0;

    if (!m_graphicsMode) {
        if (data.size() >= 2 && static_cast<uint8_t>(data[0]) == 0xFF
            && static_cast<uint8_t>(data[1]) == 0xFF) {
            reset(true);
            result.changed = true;
            result.pacingResponse = true;
            return result;
        }
        m_graphicsMode = true;
        offset = 1;
    } else if (!data.isEmpty() && static_cast<uint8_t>(data[0]) == 0xFF) {
        // A second Begin Graphics while already active is a system reset only
        // when followed by the required second FF byte.
        if (data.size() >= 2 && static_cast<uint8_t>(data[1]) == 0xFF) {
            reset(true);
            result.changed = true;
            result.pacingResponse = true;
            return result;
        }
        return fail(result, 0, "unexpected Begin Graphics while graphics mode is active"), result;
    }

    bool endedWithMoreData = false;
    while (offset < data.size()) {
        const uint8_t byte = static_cast<uint8_t>(data[offset]);

        if (m_pendingOrder != PendingOrder::None) {
            if (isGraphicsData(byte)) {
                m_pendingData.append(static_cast<char>(byte));
                ++offset;
                continue;
            }
            if (byte == 0x92) {
                if (!completePending(result))
                    return result;
                ++offset;
                continue;
            }
            if (byte == 0x91) {
                endedWithMoreData = true;
                ++offset;
                break;
            }
            fail(result, offset, "variable order was not terminated by End of Data");
            return result;
        }

        switch (byte) {
        case 0x90: // End current nonspanned block; graphics mode remains active.
            offset = data.size();
            continue;
        case 0x91:
            fail(result, offset, "More Data to Come without a variable order");
            return result;
        case 0x92:
            fail(result, offset, "End of Data without a variable order");
            return result;
        case 0x93:
            m_displayEnabled = true;
            result.changed = true;
            ++offset;
            continue;
        case 0x94:
            m_displayEnabled = false;
            result.changed = true;
            ++offset;
            continue;
        case 0x95:
            m_graphicsMode = false;
            // End Graphics terminates this graphics write block. IBM i pads
            // short blocks after 0x95; those bytes are not graphics orders.
            offset = data.size();
            continue;
        case 0x96:
            m_suppressPacing = true;
            ++offset;
            continue;
        default:
            break;
        }

        auto readFixedData = [&](int count, QByteArray &values) {
            if (offset + count >= data.size())
                return fail(result, offset, "truncated fixed-length graphics order");
            for (int index = 1; index <= count; ++index) {
                const uint8_t value = static_cast<uint8_t>(data[offset + index]);
                if (!isGraphicsData(value))
                    return fail(result, offset + index, "graphics-data byte expected");
                values.append(static_cast<char>(value));
            }
            offset += count + 1;
            return true;
        };

        QByteArray values;
        switch (byte) {
        case 0x80: // Read Status
            if (!readFixedData(2, values)) return result;
            result.readStatusOffset = decodeCoordinate(
                static_cast<uint8_t>(values[0]), static_cast<uint8_t>(values[1]));
            break;
        case 0xB0: // Set Color
            if (!readFixedData(1, values)) return result;
            m_colorIndex = static_cast<uint8_t>(values[0]) & 0x3F;
            if (m_colorIndex > 7) {
                fail(result, offset - 1, "color index is outside 0..7");
                return result;
            }
            break;
        case 0xB1: // Set Style
            if (!readFixedData(4, values)) return result;
            break;
        case 0xB2: // Set Style Offset
        case 0xB3: // Set Function
        case 0xB5: // Set Marker
            if (!readFixedData(1, values)) return result;
            break;
        case 0xB4: // Set Color Table
            m_pendingOrder = PendingOrder::ColorTable;
            ++offset;
            break;
        case 0xB6: // Set Line Weight
            if (!readFixedData(1, values)) return result;
            m_lineWeight = ((static_cast<uint8_t>(values[0]) & 0x3F) == 0) ? 1 : 2;
            break;
        case 0xB7: // Set Fill Mode
            if (!readFixedData(2, values)) return result;
            break;
        case 0xA0: // Draw Polyline
            m_pendingOrder = PendingOrder::Polyline;
            ++offset;
            break;
        case 0xA1: // Draw Scanline (retained for a later rendering phase)
        case 0xA4: // Write Polymarker
        case 0xA5: // Fill Polygon
        case 0xA6: // Define Shield Area
            m_pendingOrder = PendingOrder::Ignore;
            ++offset;
            break;
        case 0xA3: { // Write Background
            if (!readFixedData(1, values)) return result;
            const int color = static_cast<uint8_t>(values[0]) & 0x3F;
            if (color > 7) {
                fail(result, offset - 1, "background color index is outside 0..7");
                return result;
            }
            m_plane.fill(m_palette[static_cast<size_t>(color)]);
            result.changed = true;
            break;
        }
        case 0xC0: // Printer Data Follows
        case 0xC2: // Load printer alphanumeric color-mix table
        case 0xC3: // Load printer graphics color-mix table
            m_pendingOrder = PendingOrder::Ignore;
            ++offset;
            break;
        case 0xC1: // Screen Copy
            ++offset;
            break;
        case 0xC4: // Set printer timeout
            if (!readFixedData(1, values)) return result;
            break;
        default:
            fail(result, offset, QString("unsupported graphics order 0x%1")
                                     .arg(byte, 2, 16, QChar('0')));
            return result;
        }
    }

    if (m_pendingOrder != PendingOrder::None && !endedWithMoreData) {
        fail(result, data.size(), "unterminated variable order at end of block");
        return result;
    }

    result.pacingResponse = !m_suppressPacing;
    return result;
}

} // namespace ui::rendering
