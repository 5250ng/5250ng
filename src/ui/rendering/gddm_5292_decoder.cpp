// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "gddm_5292_decoder.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ui::rendering {

Gddm5292Decoder::Gddm5292Decoder()
    : m_plane(kWidth, kHeight, QImage::Format_Indexed8) {
    reset(true);
}

Gddm5292Decoder::StyleCursor::StyleCursor(
    const std::array<int, 4> &style, int startSegment, int overrideLength)
    : lengths(style), segment(startSegment & 0x03) {
    remaining = overrideLength > 0 ? overrideLength : lengths[segment];
}

bool Gddm5292Decoder::StyleCursor::take() {
    int emptySegments = 0;
    while (remaining == 0 && emptySegments < 4) {
        segment = (segment + 1) & 0x03;
        remaining = lengths[segment];
        ++emptySegments;
    }
    if (remaining == 0)
        return false;

    const bool visible = (segment & 1) == 0;
    --remaining;
    if (remaining == 0) {
        segment = (segment + 1) & 0x03;
        remaining = lengths[segment];
    }
    return visible;
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
    m_lineStyle = {15, 0, 15, 0};
    m_styleStartSegment = 0;
    m_styleOverrideLength = 0;
    m_marker = 0;
    m_fillReference = FillReference::Vertical;
    m_fillMode = FillMode::SolidBoundaryAndFill;
    m_fillReferenceShift = 0;
    // The 5292 default is three five-and-a-half-second time units.
    m_printerTimeout = 3;
    m_rasterFunction = RasterFunction::Replace;
    m_lastError = ErrorCode::None;
    m_lastErrorOffset = -1;
    m_shieldAreas.clear();
    m_shieldNonHorizontalEdges = 0;
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
    m_shieldAreas.clear();
    m_shieldNonHorizontalEdges = 0;
    return false;
}

void Gddm5292Decoder::recover(Result &result, ErrorCode code, int offset,
                              const QString &message) {
    m_lastError = code;
    m_lastErrorOffset = std::max(0, offset);
    result.error = true;
    result.errorMessage = QString("G%1 at offset %2: %3")
                              .arg(static_cast<int>(code)).arg(offset).arg(message);
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

void Gddm5292Decoder::drawLine(int x0, int y0, int x1, int y1,
                               StyleCursor *style, bool skipFirst,
                               bool weighted) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    bool first = true;

    for (;;) {
        if (!(skipFirst && first)) {
            const bool visible = style == nullptr || style->take();
            if (visible) {
                const int weight = weighted ? m_lineWeight : 1;
                for (int weightY = 0; weightY < weight; ++weightY) {
                    for (int weightX = 0; weightX < weight; ++weightX)
                        writePel(x0 + weightX, y0 + weightY, m_colorIndex);
                }
            }
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
        first = false;
    }
}

bool Gddm5292Decoder::decodePoints(Result &result, int minimumPoints,
                                   const QString &orderName,
                                   QVector<QPoint> &points) {
    if (m_pendingData.size() < minimumPoints * 4
        || (m_pendingData.size() % 4) != 0) {
        return fail(result, ErrorCode::G1, m_pendingData.size(),
                    QString("%1 requires at least %2 complete coordinate pair(s)")
                        .arg(orderName).arg(minimumPoints));
    }

    points.reserve(m_pendingData.size() / 4);
    for (int offset = 0; offset < m_pendingData.size(); offset += 4) {
        const int x = decodeCoordinate(static_cast<uint8_t>(m_pendingData[offset]),
                                       static_cast<uint8_t>(m_pendingData[offset + 1]));
        const int graphicsY = decodeCoordinate(
            static_cast<uint8_t>(m_pendingData[offset + 2]),
            static_cast<uint8_t>(m_pendingData[offset + 3]));
        if (x >= kWidth || graphicsY >= kHeight) {
            fail(result, ErrorCode::G1, offset,
                 "coordinate is outside the 480x288 PEL buffer");
            return false;
        }
        points.append(QPoint(x, kHeight - 1 - graphicsY));
    }
    return true;
}

bool Gddm5292Decoder::drawPolyline(Result &result) {
    QVector<QPoint> points;
    if (!decodePoints(result, 2, "polyline", points))
        return false;

    StyleCursor style(m_lineStyle, m_styleStartSegment, m_styleOverrideLength);
    for (int index = 1; index < points.size(); ++index) {
        const QPoint &previous = points[index - 1];
        const QPoint &next = points[index];
        drawLine(previous.x(), previous.y(), next.x(), next.y(), &style, index > 1);
    }
    result.changed = true;
    return true;
}

bool Gddm5292Decoder::drawScanline(Result &result) {
    if (m_pendingData.size() < 5) {
        return fail(result, ErrorCode::G1, m_pendingData.size(),
                    "scanline requires a coordinate and at least one PEL-pattern byte");
    }

    int x = decodeCoordinate(static_cast<uint8_t>(m_pendingData[0]),
                             static_cast<uint8_t>(m_pendingData[1]));
    const int graphicsY = decodeCoordinate(static_cast<uint8_t>(m_pendingData[2]),
                                           static_cast<uint8_t>(m_pendingData[3]));
    if (x >= kWidth || graphicsY >= kHeight) {
        return fail(result, ErrorCode::G1, 0,
                    "scanline coordinate is outside the 480x288 PEL buffer");
    }

    const int y = kHeight - 1 - graphicsY;
    for (int offset = 4; offset < m_pendingData.size(); ++offset) {
        const uint8_t pattern = static_cast<uint8_t>(m_pendingData[offset]) & 0x3F;
        for (int bit = 5; bit >= 0; --bit, ++x) {
            if ((pattern & (1U << bit)) != 0) {
                writePel(x, y, m_colorIndex);
                result.changed = true;
            }
        }
    }
    return true;
}

void Gddm5292Decoder::drawMarker(int x, int y) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int manhattan = std::abs(dx) + std::abs(dy);
            bool draw = false;
            switch (m_marker) {
            case 0: draw = true; break;
            case 1: draw = std::abs(dx) <= 1 && std::abs(dy) <= 1; break;
            case 2: draw = std::abs(dx) == 2 || std::abs(dy) == 2; break;
            case 3: draw = dx == 0 || dy == 0; break;
            case 4: draw = std::abs(dx) == std::abs(dy); break;
            case 5: draw = manhattan <= 2; break;
            case 6: draw = manhattan == 2; break;
            case 7: draw = dx == 0 || std::abs(dx) == std::abs(dy); break;
            case 8: draw = dx == 0 || dy == 0 || std::abs(dx) == std::abs(dy); break;
            }
            if (draw)
                writePel(x + dx, y + dy, m_colorIndex);
        }
    }
}

bool Gddm5292Decoder::drawPolymarker(Result &result) {
    QVector<QPoint> points;
    if (!decodePoints(result, 1, "polymarker", points))
        return false;

    const int radius = m_marker == 1 ? 1 : 2;
    for (int index = 0; index < points.size(); ++index) {
        const QPoint &point = points[index];
        if (point.x() < radius || point.x() >= kWidth - radius
            || point.y() < radius || point.y() >= kHeight - radius) {
            recover(result, ErrorCode::G5, index * 4,
                    "marker center does not allow the entire marker to be drawn");
            continue;
        }
        drawMarker(point.x(), point.y());
        result.changed = true;
    }
    return true;
}

int Gddm5292Decoder::nonHorizontalEdges(const QPolygon &polygon) {
    int count = 0;
    for (int index = 0; index < polygon.size(); ++index) {
        const QPoint &first = polygon[index];
        const QPoint &second = polygon[(index + 1) % polygon.size()];
        if (first.y() != second.y())
            ++count;
    }
    return count;
}

bool Gddm5292Decoder::pointOnPolygon(const QPoint &point,
                                     const QPolygon &polygon) {
    for (int index = 0; index < polygon.size(); ++index) {
        const QPoint &first = polygon[index];
        const QPoint &second = polygon[(index + 1) % polygon.size()];
        const qint64 cross = qint64(point.x() - first.x()) * (second.y() - first.y())
                           - qint64(point.y() - first.y()) * (second.x() - first.x());
        if (cross == 0 && point.x() >= std::min(first.x(), second.x())
            && point.x() <= std::max(first.x(), second.x())
            && point.y() >= std::min(first.y(), second.y())
            && point.y() <= std::max(first.y(), second.y()))
            return true;
    }
    return false;
}

bool Gddm5292Decoder::isShielded(const QPoint &point) const {
    bool shielded = false;
    for (const QPolygon &shield : m_shieldAreas) {
        if (pointOnPolygon(point, shield))
            return true;
        if (shield.containsPoint(point, Qt::OddEvenFill))
            shielded = !shielded;
    }
    return shielded;
}

bool Gddm5292Decoder::fillStyleVisible(int graphicsX, int graphicsY,
                                       const QPolygon &polygon) const {
    int distance = 0;
    switch (m_fillReference) {
    case FillReference::Vertical:
        distance = graphicsX + m_fillReferenceShift;
        break;
    case FillReference::Positive45:
        distance = graphicsX - graphicsY + kHeight + m_fillReferenceShift;
        break;
    case FillReference::Negative45:
        distance = graphicsX + graphicsY + m_fillReferenceShift;
        break;
    case FillReference::PolygonEdge: {
        const QPoint &first = polygon[0];
        const QPoint &second = polygon[1];
        const int firstGraphicsY = kHeight - 1 - first.y();
        const int secondGraphicsY = kHeight - 1 - second.y();
        const int dx = second.x() - first.x();
        const int dy = secondGraphicsY - firstGraphicsY;
        const qint64 cross = qint64(dx) * (graphicsY - firstGraphicsY)
                           - qint64(dy) * (graphicsX - first.x());
        const int scale = std::max(1, std::max(std::abs(dx), std::abs(dy)));
        distance = int(std::abs(cross) / scale) + m_fillReferenceShift;
        break;
    }
    }

    distance = std::max(0, distance);
    const int firstLength = m_styleOverrideLength > 0
                          ? m_styleOverrideLength
                          : m_lineStyle[static_cast<size_t>(m_styleStartSegment)];
    if (distance < firstLength)
        return (m_styleStartSegment & 1) == 0;
    distance -= firstLength;

    const int cycleLength = m_lineStyle[0] + m_lineStyle[1]
                          + m_lineStyle[2] + m_lineStyle[3];
    if (cycleLength == 0)
        return false;
    distance %= cycleLength;
    for (int count = 0, segment = (m_styleStartSegment + 1) & 0x03;
         count < 4; ++count, segment = (segment + 1) & 0x03) {
        const int length = m_lineStyle[static_cast<size_t>(segment)];
        if (distance < length)
            return (segment & 1) == 0;
        distance -= length;
    }
    return false;
}

void Gddm5292Decoder::drawPolygonBoundary(const QPolygon &polygon, bool styled) {
    StyleCursor style(m_lineStyle, m_styleStartSegment, m_styleOverrideLength);
    for (int index = 0; index < polygon.size(); ++index) {
        const QPoint &first = polygon[index];
        const QPoint &second = polygon[(index + 1) % polygon.size()];
        drawLine(first.x(), first.y(), second.x(), second.y(), styled ? &style : nullptr,
                 index > 0, false);
    }
}

bool Gddm5292Decoder::fillPolygon(Result &result) {
    QVector<QPoint> points;
    if (!decodePoints(result, 3, "fill polygon", points))
        return false;
    const QPolygon polygon(points);
    const int edgeCount = m_shieldNonHorizontalEdges + nonHorizontalEdges(polygon);
    if (edgeCount > 128) {
        m_shieldAreas.clear();
        m_shieldNonHorizontalEdges = 0;
        return fail(result, ErrorCode::G4, 0,
                    "fill and shield areas exceed 128 nonhorizontal edges");
    }

    const bool fillInterior = m_fillMode == FillMode::SolidBoundaryAndFill
                           || m_fillMode == FillMode::FillOnly;
    const bool solidBoundary = m_fillMode == FillMode::SolidBoundaryAndFill
                            || m_fillMode == FillMode::SolidBoundaryOnly;
    const bool styledBoundary = m_fillMode == FillMode::StyledBoundaryOnly;

    if (fillInterior) {
        const QRect bounds = polygon.boundingRect().intersected(QRect(0, 0, kWidth, kHeight));
        for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                const QPoint point(x, y);
                if (pointOnPolygon(point, polygon)
                    || !polygon.containsPoint(point, Qt::OddEvenFill)
                    || isShielded(point))
                    continue;
                const int graphicsY = kHeight - 1 - y;
                if (fillStyleVisible(x, graphicsY, polygon))
                    writePel(x, y, m_colorIndex);
            }
        }
    }
    if (solidBoundary || styledBoundary)
        drawPolygonBoundary(polygon, styledBoundary);

    m_shieldAreas.clear();
    m_shieldNonHorizontalEdges = 0;
    result.changed = fillInterior || solidBoundary || styledBoundary;
    return true;
}

bool Gddm5292Decoder::defineShieldArea(Result &result) {
    QVector<QPoint> points;
    if (!decodePoints(result, 3, "shield area", points))
        return false;
    const QPolygon polygon(points);
    const int edges = nonHorizontalEdges(polygon);
    if (m_shieldNonHorizontalEdges + edges > 128) {
        return fail(result, ErrorCode::G4, 0,
                    "shield areas exceed 128 nonhorizontal edges");
    }
    m_shieldAreas.append(polygon);
    m_shieldNonHorizontalEdges += edges;
    return true;
}

bool Gddm5292Decoder::completePending(Result &result, int offset) {
    if (m_pendingData.isEmpty())
        return fail(result, ErrorCode::G1, offset,
                    "variable order requires at least one graphics-data byte");

    bool ok = true;
    if (m_pendingOrder == PendingOrder::Polyline) {
        ok = drawPolyline(result);
    } else if (m_pendingOrder == PendingOrder::Scanline) {
        ok = drawScanline(result);
    } else if (m_pendingOrder == PendingOrder::Polymarker) {
        ok = drawPolymarker(result);
    } else if (m_pendingOrder == PendingOrder::FillPolygon) {
        ok = fillPolygon(result);
    } else if (m_pendingOrder == PendingOrder::ShieldArea) {
        ok = defineShieldArea(result);
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
    } else if (m_pendingOrder == PendingOrder::PrinterData) {
        result.printerData = m_pendingData;
    } else if (m_pendingOrder == PendingOrder::PrinterColorTableAN) {
        result.printerColorTableAN = m_pendingData;
    } else if (m_pendingOrder == PendingOrder::PrinterColorTableGraphics) {
        result.printerColorTableGraphics = m_pendingData;
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
            for (int index = 0; index < 4; ++index)
                m_lineStyle[static_cast<size_t>(index)] =
                    static_cast<uint8_t>(values[index]) & 0x0F;
            break;
        case 0xB2:
            if (!readFixedData(1, values)) return result;
            m_styleStartSegment =
                (static_cast<uint8_t>(values[0]) >> 4) & 0x03;
            m_styleOverrideLength = static_cast<uint8_t>(values[0]) & 0x0F;
            break;
        case 0xB5:
            if (!readFixedData(1, values)) return result;
            m_marker = static_cast<uint8_t>(values[0]) & 0x3F;
            if (m_marker > 8) {
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
            m_lineWeight = ((static_cast<uint8_t>(values[0]) & 0x01) == 0) ? 1 : 2;
            break;
        case 0xB7:
            if (!readFixedData(2, values)) return result;
            m_fillReference = static_cast<FillReference>(
                (static_cast<uint8_t>(values[0]) >> 2) & 0x03);
            m_fillMode = static_cast<FillMode>(
                static_cast<uint8_t>(values[0]) & 0x03);
            m_fillReferenceShift = static_cast<uint8_t>(values[1]) & 0x3F;
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
                StyleCursor style(m_lineStyle, m_styleStartSegment,
                                  m_styleOverrideLength);
                for (int x = 0; x < kWidth; ++x) {
                    if (style.take())
                        writePel(x, y, color);
                }
            }
            result.changed = true;
            break;
        }
        case 0xA1:
            m_pendingOrder = PendingOrder::Scanline;
            ++offset;
            break;
        case 0xA4:
            m_pendingOrder = PendingOrder::Polymarker;
            ++offset;
            break;
        case 0xA5:
            m_pendingOrder = PendingOrder::FillPolygon;
            ++offset;
            break;
        case 0xA6:
            m_pendingOrder = PendingOrder::ShieldArea;
            ++offset;
            break;
        case 0xC0:
            m_pendingOrder = PendingOrder::PrinterData;
            ++offset;
            break;
        case 0xC2:
            m_pendingOrder = PendingOrder::PrinterColorTableAN;
            ++offset;
            break;
        case 0xC3:
            m_pendingOrder = PendingOrder::PrinterColorTableGraphics;
            ++offset;
            break;
        case 0xC1:
            result.screenCopyRequested = true;
            ++offset;
            break;
        case 0xC4:
            if (!readFixedData(1, values)) return result;
            m_printerTimeout = static_cast<uint8_t>(values[0]) & 0x3F;
            result.printerTimeout = m_printerTimeout;
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

    result.completion = m_suppressPacing ? Completion::None
                                         : (result.error ? Completion::RecoverableError
                                                         : Completion::Success);
    return result;
}

} // namespace ui::rendering
