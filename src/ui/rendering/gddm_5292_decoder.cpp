// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "gddm_5292_decoder.h"

#include <algorithm>
#include <cstdlib>

namespace ui::rendering {

Gddm5292Decoder::Gddm5292Decoder()
    : m_plane(kWidth, kHeight, QImage::Format_Indexed8) {
    reset(true);
}

void Gddm5292Decoder::applyPalette() {
    QVector<QRgb> table(256, qRgb(0, 0, 0));
    for (size_t index = 0; index < m_palette.size(); ++index)
        table[static_cast<int>(index)] = m_palette[index].rgb();
    m_plane.setColorTable(table);
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
    m_rasterFunction = RasterFunction::Replace;
    m_lastError = ErrorCode::None;
    m_lastErrorOffset = -1;
    applyPalette();
    if (clearPlane)
        m_plane.fill(0);
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

int Gddm5292Decoder::decodeBufferOffset(uint8_t high, uint8_t low) {
    return ((high & 0x3F) << 6) | (low & 0x3F);
}

QByteArray Gddm5292Decoder::encodedErrorCode(ErrorCode code) {
    if (code == ErrorCode::None)
        return QByteArray::fromHex("ffff");
    QByteArray encoded;
    encoded.reserve(2);
    encoded.append(static_cast<char>(0xC7));
    encoded.append(static_cast<char>(0xF0 + static_cast<int>(code)));
    return encoded;
}

QByteArray Gddm5292Decoder::statusBytes() const {
    QByteArray status;
    status.reserve(20);
    status.append(encodedErrorCode(m_lastError));
    status.append(QByteArray::fromHex("fff280"));
    if (m_lastErrorOffset < 0) {
        status.append(QByteArray::fromHex("ffff"));
    } else {
        status.append(static_cast<char>((m_lastErrorOffset >> 8) & 0xFF));
        status.append(static_cast<char>(m_lastErrorOffset & 0xFF));
    }
    status.append(QByteArray(13, static_cast<char>(0x40)));
    return status;
}

bool Gddm5292Decoder::fail(Result &result, ErrorCode code, int offset,
                           const QString &message) {
    m_lastError = code;
    m_lastErrorOffset = std::max(0, offset);
    result.error = true;
    result.errorMessage = QString("G%1 at offset %2: %3")
                              .arg(static_cast<int>(code)).arg(offset).arg(message);
    result.completion = m_suppressPacing ? Completion::None : Completion::FatalError;
    m_graphicsMode = false;
    m_pendingOrder = PendingOrder::None;
    m_pendingData.clear();
    return false;
}

void Gddm5292Decoder::writePel(int x, int y, int colorIndex) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight)
        return;

    auto *line = m_plane.scanLine(y);
    const int previous = line[x] & 0x07;
    int next = colorIndex & 0x07;
    switch (m_rasterFunction) {
    case RasterFunction::Or:
        next = previous | next;
        break;
    case RasterFunction::Xor:
        next = previous ^ next;
        break;
    case RasterFunction::Replace:
        break;
    }
    line[x] = static_cast<uchar>(next);
}

void Gddm5292Decoder::drawLine(int x0, int y0, int x1, int y1) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        for (int weightY = 0; weightY < m_lineWeight; ++weightY) {
            for (int weightX = 0; weightX < m_lineWeight; ++weightX)
                writePel(x0 + weightX, y0 + weightY, m_colorIndex);
        }
        if (x0 == x1 && y0 == y1)
            break;
        const int twiceError = 2 * error;
        if (twiceError >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twiceError <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

bool Gddm5292Decoder::drawPolyline(Result &result) {
    if (m_pendingData.size() < 8 || (m_pendingData.size() % 4) != 0) {
        return fail(result, ErrorCode::G1, m_pendingData.size(),
                    "polyline requires complete coordinate pairs");
    }

    auto pointAt = [this, &result](int offset, int &x, int &y) {
        x = decodeCoordinate(static_cast<uint8_t>(m_pendingData[offset]),
                             static_cast<uint8_t>(m_pendingData[offset + 1]));
        const int graphicsY = decodeCoordinate(
            static_cast<uint8_t>(m_pendingData[offset + 2]),
            static_cast<uint8_t>(m_pendingData[offset + 3]));
        if (x >= kWidth || graphicsY >= kHeight) {
            fail(result, ErrorCode::G1, offset, "coordinate is outside the 480x288 PEL buffer");
            return false;
        }
        y = kHeight - 1 - graphicsY;
        return true;
    };

    int previousX = 0;
    int previousY = 0;
    if (!pointAt(0, previousX, previousY))
        return false;
    for (int offset = 4; offset < m_pendingData.size(); offset += 4) {
        int nextX = 0;
        int nextY = 0;
        if (!pointAt(offset, nextX, nextY))
            return false;
        drawLine(previousX, previousY, nextX, nextY);
        previousX = nextX;
        previousY = nextY;
    }
    result.changed = true;
    return true;
}

bool Gddm5292Decoder::completePending(Result &result, int offset) {
    if (m_pendingData.isEmpty())
        return fail(result, ErrorCode::G1, offset,
                    "variable order requires at least one graphics-data byte");

    bool ok = true;
    if (m_pendingOrder == PendingOrder::Polyline) {
        ok = drawPolyline(result);
    } else if (m_pendingOrder == PendingOrder::ColorTable) {
        if ((m_pendingData.size() % 3) != 0) {
            ok = fail(result, ErrorCode::G1, offset,
                      "color table requires index/red-green/blue triples");
        } else {
            for (int dataOffset = 0; dataOffset < m_pendingData.size(); dataOffset += 3) {
                const int index = static_cast<uint8_t>(m_pendingData[dataOffset]) & 0x3F;
                if (index == 0 || index > 7) {
                    ok = fail(result, ErrorCode::G3, dataOffset,
                              "color table index must be in 1..7");
                    break;
                }
                const uint8_t redGreen = static_cast<uint8_t>(m_pendingData[dataOffset + 1]);
                const uint8_t blue = static_cast<uint8_t>(m_pendingData[dataOffset + 2]);
                const int redIntensity = (redGreen >> 3) & 0x07;
                const int greenIntensity = redGreen & 0x07;
                const int blueIntensity = (blue >> 3) & 0x07;
                m_palette[static_cast<size_t>(index)] =
                    QColor(redIntensity * 255 / 7, greenIntensity * 255 / 7,
                           blueIntensity * 255 / 7);
            }
            if (ok) {
                applyPalette();
                result.changed = true;
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
            result.completion = Completion::SystemReset;
            return result;
        }
        m_graphicsMode = true;
        offset = 1;
    } else if (!data.isEmpty() && static_cast<uint8_t>(data[0]) == 0xFF) {
        if (data.size() >= 2 && static_cast<uint8_t>(data[1]) == 0xFF) {
            reset(true);
            result.changed = true;
            result.completion = Completion::SystemReset;
            return result;
        }
        fail(result, ErrorCode::G1, 0,
             "unexpected Begin Graphics while graphics mode is active");
        return result;
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
                if (!completePending(result, offset))
                    return result;
                ++offset;
                continue;
            }
            if (byte == 0x91) {
                if (m_pendingData.isEmpty()) {
                    fail(result, ErrorCode::G1, offset,
                         "More Data to Come requires graphics data");
                    return result;
                }
                endedWithMoreData = true;
                ++offset;
                break;
            }
            fail(result, ErrorCode::G1, offset,
                 "variable order was not terminated by End of Data");
            return result;
        }

        switch (byte) {
        case 0x90:
            offset = data.size();
            continue;
        case 0x91:
            fail(result, ErrorCode::G1, offset,
                 "More Data to Come without a variable order");
            return result;
        case 0x92:
            fail(result, ErrorCode::G1, offset, "End of Data without a variable order");
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
                return fail(result, ErrorCode::G1, offset,
                            "truncated fixed-length graphics order");
            for (int index = 1; index <= count; ++index) {
                const uint8_t value = static_cast<uint8_t>(data[offset + index]);
                if (!isGraphicsData(value))
                    return fail(result, ErrorCode::G1, offset + index,
                                "graphics-data byte expected");
                values.append(static_cast<char>(value));
            }
            offset += count + 1;
            return true;
        };

        QByteArray values;
        switch (byte) {
        case 0x80: {
            if (!readFixedData(2, values)) return result;
            const int statusOffset = decodeBufferOffset(
                static_cast<uint8_t>(values[0]), static_cast<uint8_t>(values[1]));
            if (statusOffset > 1919) {
                fail(result, ErrorCode::G1, offset - 2,
                     "Read Status offset is outside the 1920-byte alphanumeric buffer");
                return result;
            }
            result.statusWrites.append({statusOffset, statusBytes()});
            break;
        }
        case 0xB0:
            if (!readFixedData(1, values)) return result;
            m_colorIndex = static_cast<uint8_t>(values[0]) & 0x3F;
            if (m_colorIndex > 7) {
                fail(result, ErrorCode::G3, offset - 1, "color index must be in 0..7");
                return result;
            }
            break;
        case 0xB1:
            if (!readFixedData(4, values)) return result;
            break;
        case 0xB2:
            if (!readFixedData(1, values)) return result;
            break;
        case 0xB5:
            if (!readFixedData(1, values)) return result;
            if ((static_cast<uint8_t>(values[0]) & 0x3F) > 8) {
                fail(result, ErrorCode::G3, offset - 1, "marker index must be in 0..8");
                return result;
            }
            break;
        case 0xB3: {
            if (!readFixedData(1, values)) return result;
            const int function = static_cast<uint8_t>(values[0]) & 0x03;
            if (function < static_cast<int>(RasterFunction::Or)
                || function > static_cast<int>(RasterFunction::Replace)) {
                fail(result, ErrorCode::G3, offset - 1, "invalid raster function");
                return result;
            }
            m_rasterFunction = static_cast<RasterFunction>(function);
            break;
        }
        case 0xB4:
            m_pendingOrder = PendingOrder::ColorTable;
            ++offset;
            break;
        case 0xB6:
            if (!readFixedData(1, values)) return result;
            m_lineWeight = ((static_cast<uint8_t>(values[0]) & 0x3F) == 0) ? 1 : 2;
            break;
        case 0xB7:
            if (!readFixedData(2, values)) return result;
            break;
        case 0xA0:
            m_pendingOrder = PendingOrder::Polyline;
            ++offset;
            break;
        case 0xA3: {
            if (!readFixedData(1, values)) return result;
            const int color = static_cast<uint8_t>(values[0]) & 0x3F;
            if (color > 7) {
                fail(result, ErrorCode::G3, offset - 1,
                     "background color index must be in 0..7");
                return result;
            }
            for (int y = 0; y < kHeight; ++y) {
                for (int x = 0; x < kWidth; ++x)
                    writePel(x, y, color);
            }
            result.changed = true;
            break;
        }
        case 0xA1:
        case 0xA4:
        case 0xA5:
        case 0xA6:
            fail(result, ErrorCode::G2, offset, QString("unsupported drawing order 0x%1")
                                                    .arg(byte, 2, 16, QChar('0')));
            return result;
        case 0xC0:
        case 0xC2:
        case 0xC3:
            m_pendingOrder = PendingOrder::Ignore;
            ++offset;
            break;
        case 0xC1:
            ++offset;
            break;
        case 0xC4:
            if (!readFixedData(1, values)) return result;
            break;
        default:
            fail(result, ErrorCode::G2, offset, QString("unsupported graphics order 0x%1")
                                                    .arg(byte, 2, 16, QChar('0')));
            return result;
        }
    }

    if (m_pendingOrder != PendingOrder::None && !endedWithMoreData) {
        fail(result, ErrorCode::G1, data.size(),
             "unterminated variable order at end of block");
        return result;
    }

    result.completion = m_suppressPacing ? Completion::None : Completion::Success;
    return result;
}

} // namespace ui::rendering
