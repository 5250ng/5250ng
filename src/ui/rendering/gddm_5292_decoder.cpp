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
    m_style = LineStyle();
    m_styleOffset = StyleOffset();
    m_fillMode = FillMode();
    m_marker = 0;
    // Default Printer A/N Color Mix Table, as bcmy nibbles. Every fourth entry
    // is a fixed "no ink" the host cannot change.
    m_printerAlphaMix = {8, 8, 5, 5, 8, 8, 5, 0,
                         2, 2, 2, 2, 2, 2, 2, 0,
                         4, 4, 1, 1, 4, 4, 1, 0,
                         3, 3, 6, 6, 3, 3, 6, 0};
    // Default Printer Graphics Color Mix Table.
    m_printerGraphicsMix = {0, 2, 5, 6, 3, 1, 4, 8};
    m_printerTimeoutUnits = 3;
    m_printerData.clear();
    m_lastError = ErrorCode::None;
    m_lastErrorOffset = -1;
    m_lastOrder = 0;
    m_lastBlock.clear();
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

const char *Gddm5292Decoder::errorCodeText(ErrorCode code) {
    switch (code) {
    case ErrorCode::G1: return "G1";
    case ErrorCode::G2: return "G2";
    case ErrorCode::G3: return "G3";
    case ErrorCode::G4: return "G4";
    case ErrorCode::G5: return "G5";
    case ErrorCode::P5: return "P5";
    case ErrorCode::None: break;
    }
    return "";
}

QByteArray Gddm5292Decoder::encodedErrorCode(ErrorCode code) {
    const char *text = errorCodeText(code);
    if (text[0] == '\0')
        return QByteArray::fromHex("ffff");
    // The code appears on the device's status line, so it travels to the
    // alphanumeric buffer as display characters: EBCDIC letter then digit.
    QByteArray encoded;
    encoded.reserve(2);
    encoded.append(static_cast<char>(text[0] == 'P' ? 0xD7 : 0xC7));
    encoded.append(static_cast<char>(0xF0 + (text[1] - '0')));
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
    result.errorMessage = QString("%1 at offset %2: %3")
                              .arg(errorCodeText(code)).arg(offset).arg(message);
    result.completion = m_suppressPacing ? Completion::None : Completion::FatalError;
    result.pacingSuppressed = m_suppressPacing;
    m_graphicsMode = false;
    m_pendingOrder = PendingOrder::None;
    m_pendingData.clear();
    return false;
}

void Gddm5292Decoder::warn(Result &result, const QString &message) {
    if (result.warning.isEmpty())
        result.warning = message;
    else if (!result.warning.contains(message))
        result.warning += QStringLiteral("; ") + message;
}

void Gddm5292Decoder::noteRecoverable(Result &result, ErrorCode code, int offset,
                                     const QString &message) {
    m_lastError = code;
    m_lastErrorOffset = std::max(0, offset);
    result.error = true;
    result.errorMessage = QString("%1 at offset %2: %3")
                              .arg(errorCodeText(code)).arg(offset).arg(message);
    // The completion is decided at the end of the block: the device shows the
    // code, sounds the alarm, finishes processing and only then answers Cmd-9.
}

bool Gddm5292Decoder::LineStyle::covers(int offset, int segment,
                                        int overrideLength) const {
    // The style is four alternating runs: visible, gap, visible, gap. Set Style
    // Offset picks which run a line starts on and may override its length.
    const int lengths[4] = {visible1, gap1, visible2, gap2};
    const bool visible[4] = {true, false, true, false};
    if (period() <= 0 || offset < 0)
        return true;

    int run = segment & 0x03;
    int remaining = overrideLength > 0 ? overrideLength : lengths[run];
    // period() > 0 guarantees a nonzero run, so this terminates; the bound is
    // belt and braces against a hostile style.
    for (int guard = 0; guard < 4096; ++guard) {
        if (remaining > 0) {
            if (offset < remaining)
                return visible[run];
            offset -= remaining;
        }
        run = (run + 1) & 0x03;
        remaining = lengths[run];
    }
    return true;
}

int Gddm5292Decoder::stylePhase(int x, int deviceY) const {
    // The style runs across the fill lines, so the offset advances
    // perpendicular to the direction those lines take. A diagonal line keeps
    // x - y (or x + y) constant along itself, so that difference is what
    // advances across them. Which diagonal the manual calls "+45 degrees from
    // vertical" is unsettled: no captured stream has requested either, so the
    // two may be swapped. The vertical reference, all IBM i has been seen to
    // use, is unaffected.
    int phase = x;
    switch (m_fillMode.reference) {
    case FillReference::Vertical:
        phase = x;
        break;
    case FillReference::Plus45:
        phase = x - deviceY;
        break;
    case FillReference::Minus45:
        phase = x + deviceY;
        break;
    case FillReference::PolygonEdge:
        // Following the polygon edge needs per-edge distance fields, which no
        // captured stream asks for. Approximated by the vertical reference
        // rather than refusing the order.
        phase = x;
        break;
    }
    return phase + m_fillMode.referenceShift;
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

void Gddm5292Decoder::drawLine(int x0, int y0, int x1, int y1, bool styled) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int along = 0;

    for (;;) {
        if (!styled || m_style.covers(along, m_styleOffset.segment,
                                      m_styleOffset.overrideLength)) {
            for (int weightY = 0; weightY < m_lineWeight; ++weightY) {
                for (int weightX = 0; weightX < m_lineWeight; ++weightX)
                    writePel(x0 + weightX, y0 + weightY, m_colorIndex);
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
        ++along;
    }
}

bool Gddm5292Decoder::decodePoints(Result &result, int offset, const char *what,
                                  int minimumPoints, QVector<QPoint> *out) {
    if (m_pendingData.size() < minimumPoints * 4 || (m_pendingData.size() % 4) != 0) {
        return fail(result, ErrorCode::G1, offset,
                    QString("%1 requires at least %2 complete coordinate pairs")
                        .arg(what).arg(minimumPoints));
    }

    out->clear();
    out->reserve(m_pendingData.size() / 4);
    for (int index = 0; index < m_pendingData.size(); index += 4) {
        const int x = decodeCoordinate(static_cast<uint8_t>(m_pendingData[index]),
                                       static_cast<uint8_t>(m_pendingData[index + 1]));
        const int graphicsY = decodeCoordinate(
            static_cast<uint8_t>(m_pendingData[index + 2]),
            static_cast<uint8_t>(m_pendingData[index + 3]));
        if (x >= kWidth || graphicsY >= kHeight) {
            return fail(result, ErrorCode::G1, offset,
                        "coordinate is outside the 480x288 PEL buffer");
        }
        out->append(QPoint(x, kHeight - 1 - graphicsY));
    }
    return true;
}

bool Gddm5292Decoder::drawPolyline(Result &result) {
    QVector<QPoint> points;
    if (!decodePoints(result, m_pendingData.size(), "polyline", 2, &points))
        return false;

    for (int index = 1; index < points.size(); ++index) {
        drawLine(points[index - 1].x(), points[index - 1].y(),
                 points[index].x(), points[index].y());
    }
    result.changed = true;
    return true;
}

void Gddm5292Decoder::drawClosedOutline(const QVector<QPoint> &points, bool styled) {
    for (int index = 0; index < points.size(); ++index) {
        const QPoint &from = points[index];
        const QPoint &to = points[(index + 1) % points.size()];
        drawLine(from.x(), from.y(), to.x(), to.y(), styled);
    }
}

namespace {

// 5x5 marker shapes, bit 4 of each row being the leftmost PEL, indexed by the
// Set Marker value. Verified against the Functions Reference's glyph column,
// rendered from the PDF because it does not survive text extraction: solid 5x5
// box, solid 3x3 box, empty 5x5 box, plus, cross, solid diamond, hollow
// diamond, three-segment asterisk, four-segment asterisk.
constexpr uint8_t kMarkerShapes[9][5] = {
    {0b11111, 0b11111, 0b11111, 0b11111, 0b11111},
    {0b00000, 0b01110, 0b01110, 0b01110, 0b00000},
    {0b11111, 0b10001, 0b10001, 0b10001, 0b11111},
    {0b00100, 0b00100, 0b11111, 0b00100, 0b00100},
    {0b10001, 0b01010, 0b00100, 0b01010, 0b10001},
    {0b00100, 0b01110, 0b11111, 0b01110, 0b00100},
    {0b00100, 0b01010, 0b10001, 0b01010, 0b00100},
    {0b10101, 0b01110, 0b00100, 0b01110, 0b10101},
    {0b10101, 0b01110, 0b11111, 0b01110, 0b10101},
};

} // namespace

const uint8_t *Gddm5292Decoder::markerShape() const {
    const int index = (m_marker >= 0 && m_marker < kMarkerCount) ? m_marker : 0;
    return kMarkerShapes[index];
}

// Draw Scanline (order A1). Two coordinate pairs give the leftmost PEL, then
// the pattern follows. Each graphics-data byte carries six pattern bits, most
// significant first, advancing in +X; a 1 sets the PEL using the current colour
// and function and a 0 leaves it alone. So an 8-PEL row arrives as two bytes
// with the second zero-padded, which is what a capture of GSIMG with a known
// bit pattern confirms. GDDM uses this for images and for mode-2 text.
bool Gddm5292Decoder::drawScanline(Result &result, int offset) {
    if (m_pendingData.size() < 5) {
        return fail(result, ErrorCode::G1, offset,
                    "scanline requires a coordinate pair and a pattern byte");
    }

    const int x = decodeCoordinate(static_cast<uint8_t>(m_pendingData[0]),
                                   static_cast<uint8_t>(m_pendingData[1]));
    const int graphicsY = decodeCoordinate(static_cast<uint8_t>(m_pendingData[2]),
                                           static_cast<uint8_t>(m_pendingData[3]));
    if (x >= kWidth || graphicsY >= kHeight) {
        return fail(result, ErrorCode::G1, offset,
                    "coordinate is outside the 480x288 PEL buffer");
    }
    const int y = kHeight - 1 - graphicsY;

    int pel = 0;
    for (int index = 4; index < m_pendingData.size(); ++index) {
        const uint8_t bits = static_cast<uint8_t>(m_pendingData[index]) & 0x3F;
        for (int bit = 5; bit >= 0; --bit, ++pel) {
            if ((bits >> bit) & 0x01)
                writePel(x + pel, y, m_colorIndex);
        }
    }
    result.changed = true;
    return true;
}

// Write Polymarker (order A4). Draws the marker selected by Set Marker centred
// on each coordinate pair. A marker whose box will not fit raises G5, the one
// recoverable graphics error: the device skips it, finishes the block and
// answers Cmd-9. Centring is confirmed by GDDM's own scanline fallback, which
// places an equivalent marker's top-left at the centre minus two.
bool Gddm5292Decoder::drawPolymarker(Result &result, int offset) {
    QVector<QPoint> points;
    if (!decodePoints(result, offset, "polymarker", 1, &points))
        return false;

    const uint8_t *shape = markerShape();
    const int half = kMarkerSize / 2;

    for (const QPoint &centre : points) {
        if (centre.x() - half < 0 || centre.x() + half >= kWidth
            || centre.y() - half < 0 || centre.y() + half >= kHeight) {
            noteRecoverable(result, ErrorCode::G5, offset,
                            QString("marker at (%1,%2) does not fit inside the display")
                                .arg(centre.x()).arg(kHeight - 1 - centre.y()));
            continue;
        }
        for (int row = 0; row < kMarkerSize; ++row) {
            for (int column = 0; column < kMarkerSize; ++column) {
                if ((shape[row] >> (kMarkerSize - 1 - column)) & 0x01) {
                    writePel(centre.x() - half + column,
                             centre.y() - half + row, m_colorIndex);
                }
            }
        }
        result.changed = true;
    }
    return true;
}

// Fill Polygon (order A5). The last vertex closes back onto the first, and the
// interior is shaded by the even-odd rule: a PEL is inside when a ray leaving
// it crosses an odd number of boundary lines. IBM i relies on the device to
// work the interior out, so a convex or nonzero-winding fill is not a valid
// substitute. The interior is painted through the current style, which is what
// makes a non-solid GSPAT arrive as a hatch.
bool Gddm5292Decoder::fillPolygon(Result &result, int offset) {
    QVector<QPoint> points;
    if (!decodePoints(result, offset, "fill polygon", 3, &points))
        return false;

    int nonHorizontal = 0;
    for (int index = 0; index < points.size(); ++index) {
        if (points[index].y() != points[(index + 1) % points.size()].y())
            ++nonHorizontal;
    }
    if (nonHorizontal > kMaxFillEdges) {
        return fail(result, ErrorCode::G4, offset,
                    QString("fill polygon has %1 nonhorizontal edges, the device "
                            "limit is %2").arg(nonHorizontal).arg(kMaxFillEdges));
    }

    if (m_fillMode.interior) {
        int top = points.front().y();
        int bottom = top;
        for (const QPoint &point : points) {
            top = qMin(top, point.y());
            bottom = qMax(bottom, point.y());
        }
        top = qMax(top, 0);
        bottom = qMin(bottom, kHeight - 1);

        QVector<double> crossings;
        for (int y = top; y <= bottom; ++y) {
            // Sample at the PEL centre so a vertex sitting exactly on the scan
            // line is counted once rather than twice.
            const double scan = y + 0.5;
            crossings.clear();
            for (int index = 0; index < points.size(); ++index) {
                const QPoint &from = points[index];
                const QPoint &to = points[(index + 1) % points.size()];
                const double y0 = from.y();
                const double y1 = to.y();
                if ((y0 <= scan && y1 > scan) || (y1 <= scan && y0 > scan)) {
                    const double along = (scan - y0) / (y1 - y0);
                    crossings.append(from.x() + along * (to.x() - from.x()));
                }
            }
            if (crossings.size() < 2)
                continue;
            std::sort(crossings.begin(), crossings.end());

            const int deviceY = kHeight - 1 - y;
            for (int pair = 0; pair + 1 < crossings.size(); pair += 2) {
                int first = static_cast<int>(std::ceil(crossings[pair] - 0.5));
                int last = static_cast<int>(std::floor(crossings[pair + 1] - 0.5));
                first = qMax(first, 0);
                last = qMin(last, kWidth - 1);
                for (int x = first; x <= last; ++x) {
                    if (m_style.covers(stylePhase(x, deviceY), m_styleOffset.segment,
                                       m_styleOffset.overrideLength))
                        writePel(x, y, m_colorIndex);
                }
            }
        }
        result.changed = true;
    }

    if (m_fillMode.boundary) {
        drawClosedOutline(points, m_fillMode.styledBoundary);
        result.changed = true;
    }
    return true;
}

bool Gddm5292Decoder::completePending(Result &result, int offset) {
    if (m_pendingData.isEmpty())
        return fail(result, ErrorCode::G1, offset,
                    "variable order requires at least one graphics-data byte");

    bool ok = true;
    if (m_pendingOrder == PendingOrder::Polyline) {
        ok = drawPolyline(result);
    } else if (m_pendingOrder == PendingOrder::FillPolygon) {
        ok = fillPolygon(result, offset);
    } else if (m_pendingOrder == PendingOrder::Scanline) {
        ok = drawScanline(result, offset);
    } else if (m_pendingOrder == PendingOrder::Polymarker) {
        ok = drawPolymarker(result, offset);
    } else if (m_pendingOrder == PendingOrder::GraphicsMixTable) {
        // Load Printer Graphics Color Mix Table (C3): index/bcmy pairs mapping
        // each graphics colour index onto the printer's four inks. GDDM sends
        // this in every picture's opening block, seven pairs for indexes 1..7,
        // so a screen copy prints colours matching the display palette.
        if ((m_pendingData.size() % 2) != 0) {
            ok = fail(result, ErrorCode::G1, offset,
                      "printer graphics mix table requires index/bcmy pairs");
        } else {
            for (int index = 0; index < m_pendingData.size(); index += 2) {
                const int entry = static_cast<uint8_t>(m_pendingData[index]) & 0x07;
                m_printerGraphicsMix[static_cast<size_t>(entry)] =
                    static_cast<uint8_t>(m_pendingData[index + 1]) & 0x0F;
            }
        }
    } else if (m_pendingOrder == PendingOrder::AlphaMixTable) {
        // Load Printer A/N Color Mix Table (C2). Indexes 7, 15, 23 and 31 are
        // fixed in the device, and the manual names attempting one as P5.
        if ((m_pendingData.size() % 2) != 0) {
            ok = fail(result, ErrorCode::G1, offset,
                      "printer alphanumeric mix table requires index/bcmy pairs");
        } else {
            for (int index = 0; index < m_pendingData.size(); index += 2) {
                const int entry = static_cast<uint8_t>(m_pendingData[index]) & 0x1F;
                if (alphaMixIndexIsFixed(entry)) {
                    ok = fail(result, ErrorCode::P5, offset,
                              QString("printer A/N mix table index %1 cannot be changed")
                                  .arg(entry));
                    break;
                }
                m_printerAlphaMix[static_cast<size_t>(entry)] =
                    static_cast<uint8_t>(m_pendingData[index + 1]) & 0x0F;
            }
        }
    } else if (m_pendingOrder == PendingOrder::PrinterData) {
        // Printer Data Follows (C0). With no attached printer the payload is
        // retained for a future export path, bounded so a hostile stream cannot
        // grow it without limit.
        m_printerData = m_pendingData.left(kHostMaxBlockBytes);
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

    // Keep the raw block for diagnostics, capped so a hostile stream cannot
    // grow this without bound.
    m_lastBlock = data.left(kHostMaxBlockBytes);

    if (data.size() < kMinBlockBytes) {
        warn(result, QString("block is %1 bytes, below the documented %2-byte minimum")
                         .arg(data.size()).arg(kMinBlockBytes));
    } else if (data.size() > kDeviceMaxBlockBytes) {
        warn(result, QString("block is %1 bytes, beyond the %2-byte device limit%3")
                         .arg(data.size()).arg(kDeviceMaxBlockBytes)
                         .arg(data.size() > kHostMaxBlockBytes
                                  ? QString(" and IBM i's %1-byte limit")
                                        .arg(kHostMaxBlockBytes)
                                  : QString()));
    }

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
                // More Data to Come may arrive with nothing buffered yet: IBM i
                // splits a block immediately after an order byte, leaving all
                // of that order's coordinate data for the next one.
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

        m_lastOrder = byte;
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
            m_style.visible1 = static_cast<uint8_t>(values[0]) & 0x3F;
            m_style.gap1 = static_cast<uint8_t>(values[1]) & 0x3F;
            m_style.visible2 = static_cast<uint8_t>(values[2]) & 0x3F;
            m_style.gap2 = static_cast<uint8_t>(values[3]) & 0x3F;
            break;
        case 0xB2: {
            if (!readFixedData(1, values)) return result;
            const int control = static_cast<uint8_t>(values[0]) & 0x3F;
            // Bits aa choose which Set Style run begins the line; the low four
            // bits override that run's length, zero meaning keep it.
            m_styleOffset.segment = (control >> 4) & 0x03;
            m_styleOffset.overrideLength = control & 0x0F;
            break;
        }
        case 0xB5:
            if (!readFixedData(1, values)) return result;
            // The order's bit diagram lays byte 2 out as 0100aaaa, so the
            // marker is the low four bits rather than the whole payload.
            m_marker = static_cast<uint8_t>(values[0]) & 0x0F;
            if (m_marker >= kMarkerCount) {
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
        case 0xB7: {
            if (!readFixedData(2, values)) return result;
            const int control = static_cast<uint8_t>(values[0]) & 0x3F;
            switch ((control >> 4) & 0x03) { // bits aa: fill line direction
            case 0: m_fillMode.reference = FillReference::Vertical; break;
            case 1: m_fillMode.reference = FillReference::PolygonEdge; break;
            case 2: m_fillMode.reference = FillReference::Plus45; break;
            default: m_fillMode.reference = FillReference::Minus45; break;
            }
            // Bits bb select which of the boundary and interior are drawn and
            // whether the boundary is solid or styled. The Functions Reference
            // prints "10" twice and omits "01"; this is the reading consistent
            // with the live stream, where GSAREA(1) emits bb=00 and GSAREA(0)
            // emits bb=10.
            switch (control & 0x03) {
            case 0: // solid boundary plus styled interior
                m_fillMode.boundary = true;
                m_fillMode.interior = true;
                m_fillMode.styledBoundary = false;
                break;
            case 1: // solid boundary only
                m_fillMode.boundary = true;
                m_fillMode.interior = false;
                m_fillMode.styledBoundary = false;
                break;
            case 2: // styled interior only
                m_fillMode.boundary = false;
                m_fillMode.interior = true;
                m_fillMode.styledBoundary = false;
                break;
            default: // styled boundary only
                m_fillMode.boundary = true;
                m_fillMode.interior = false;
                m_fillMode.styledBoundary = true;
                break;
            }
            m_fillMode.referenceShift = static_cast<uint8_t>(values[1]) & 0x3F;
            break;
        }
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
        case 0xA5:
            m_pendingOrder = PendingOrder::FillPolygon;
            ++offset;
            break;
        case 0xA1:
            m_pendingOrder = PendingOrder::Scanline;
            ++offset;
            break;
        case 0xA4:
            m_pendingOrder = PendingOrder::Polymarker;
            ++offset;
            break;
        case 0xA6:
            // Parsed but not rendered: IBM i GDDM has never been seen to emit
            // it, decomposing areas with holes into scanlines host-side.
            m_pendingOrder = PendingOrder::Ignore;
            ++offset;
            break;
        case 0xC0: // Printer Data Follows
            m_pendingOrder = PendingOrder::PrinterData;
            ++offset;
            break;
        case 0xC2: // Load Printer A/N Color Mix Table
            m_pendingOrder = PendingOrder::AlphaMixTable;
            ++offset;
            break;
        case 0xC3: // Load Printer Graphics Color Mix Table
            m_pendingOrder = PendingOrder::GraphicsMixTable;
            ++offset;
            break;
        case 0xC1: // Screen Copy
            // The device prints the composite of the alphanumeric buffer and
            // the graphics bitmap. Report the request and let the application
            // decide: a host should not be able to write files unprompted.
            result.screenCopyRequested = true;
            ++offset;
            break;
        case 0xC4: // Set Printer Time-Out
            if (!readFixedData(1, values)) return result;
            // One unit is 5.5 seconds, to a documented maximum of 63.
            m_printerTimeoutUnits = static_cast<uint8_t>(values[0]) & 0x3F;
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

    if (m_suppressPacing) {
        result.completion = Completion::None;
        result.pacingSuppressed = true;
    } else if (result.error) {
        // A fatal error returns early, so an error still set here was
        // recoverable: the block ran to completion and answers Cmd-9.
        result.completion = Completion::RecoverableError;
    } else {
        result.completion = Completion::Success;
    }
    return result;
}

} // namespace ui::rendering
