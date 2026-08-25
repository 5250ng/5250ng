// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QVector>
#include <array>
#include <cstdint>

namespace ui::rendering {

class Gddm5292Decoder {
  public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 288;

    // A graphics write block is documented as 11 to 256 bytes by the 5292
    // Functions Reference, while the IBM i GDDM guide allows up to 1920. The
    // difference looks like a controller layering artefact, so these bounds
    // only ever produce a diagnostic: a block is never rejected on length.
    static constexpr int kMinBlockBytes = 11;
    static constexpr int kDeviceMaxBlockBytes = 256;
    static constexpr int kHostMaxBlockBytes = 1920;

    // The device raises G4 on a fill polygon with more nonhorizontal edges.
    static constexpr int kMaxFillEdges = 128;

    // Markers are drawn from a box centred on the coordinate. One that will not
    // fit entirely on the surface raises the recoverable G5.
    static constexpr int kMarkerSize = 5;
    static constexpr int kMarkerCount = 9;

    enum class Completion {
        None,
        Success,
        SystemReset,
        RecoverableError,
        FatalError
    };

    struct StatusWrite {
        int offset = 0;
        QByteArray data;
    };

    struct Result {
        bool handled = false;
        bool changed = false;
        bool error = false;
        Completion completion = Completion::None;
        QVector<StatusWrite> statusWrites;
        QString errorMessage;
        // Non-fatal anomalies worth logging but never worth rejecting a block
        // over, such as a length outside the documented bounds.
        QString warning;
        // True when Suppress Pacing Response withheld the completion. It tells
        // a deliberately silent block from a silently broken one, which is what
        // makes the always-answer property checkable.
        bool pacingSuppressed = false;
    };

    Gddm5292Decoder();

    bool recognizes(const QByteArray &data) const;
    Result process(const QByteArray &data);

    bool graphicsMode() const { return m_graphicsMode; }
    bool displayEnabled() const { return m_displayEnabled; }
    const QImage &graphicsPlane() const { return m_plane; }
    quint64 blockCount() const { return m_blockCount; }
    QByteArray statusBytes() const;

    // Diagnostics for the log and for debugging a stream that misbehaves.
    // lastOrder is the most recent set or draw order: the control bytes 90..96
    // are handled before it and deliberately do not displace it, because "what
    // was it drawing" is the useful question.
    uint8_t lastOrder() const { return m_lastOrder; }
    const QByteArray &lastBlock() const { return m_lastBlock; }

  private:
    enum class ErrorCode {
        None,
        G1,
        G2,
        G3,
        G4,
        G5
    };

    enum class RasterFunction {
        Or = 1,
        Xor = 2,
        Replace = 3
    };

    enum class PendingOrder {
        None,
        Polyline,
        FillPolygon,
        Scanline,
        Polymarker,
        ColorTable,
        Ignore
    };

    // Set Style (order B1): alternating visible and gap run lengths in PELs.
    // The device runs these across the interior fill lines of a polygon, which
    // is what turns a fill into a hatch rather than a solid block.
    struct LineStyle {
        int visible1 = 15;
        int gap1 = 0;
        int visible2 = 15;
        int gap2 = 0;

        int period() const { return visible1 + gap1 + visible2 + gap2; }
        bool solid() const { return gap1 == 0 && gap2 == 0; }
        // `segment` is the Set Style Offset run to begin on (0..3), and
        // `overrideLength` replaces that first run's length when nonzero.
        bool covers(int offset, int segment = 0, int overrideLength = 0) const;
    };

    // Set Style Offset (order B2): which of the four Set Style runs a line
    // starts on, plus an optional override for that run's length.
    struct StyleOffset {
        int segment = 0;
        int overrideLength = 0;
    };

    // Set Fill Mode (order B7) bits aa: the direction the interior fill lines
    // run.
    enum class FillReference {
        Vertical,
        PolygonEdge,
        Plus45,
        Minus45
    };

    struct FillMode {
        FillReference reference = FillReference::Vertical;
        bool boundary = true;        // draw the polygon edge
        bool interior = true;        // shade the interior
        bool styledBoundary = false; // edge uses the current style, not solid
        int referenceShift = 0;      // bits cccccc: reference line moved left
    };

    void reset(bool clearPlane);
    bool completePending(Result &result, int offset);
    bool drawPolyline(Result &result);
    bool fillPolygon(Result &result, int offset);
    bool drawScanline(Result &result, int offset);
    bool drawPolymarker(Result &result, int offset);
    // The 5x5 shape for the current Set Marker value; bit 4 of each row is the
    // leftmost PEL.
    const uint8_t *markerShape() const;
    // Decodes m_pendingData into plane coordinates, failing G1 on a short or
    // ragged payload and on a coordinate outside the surface.
    bool decodePoints(Result &result, int offset, const char *what,
                      int minimumPoints, QVector<QPoint> *out);
    void drawClosedOutline(const QVector<QPoint> &points, bool styled);
    // Offset along the current style for the interior PEL at (x, deviceY).
    int stylePhase(int x, int deviceY) const;
    bool fail(Result &result, ErrorCode code, int offset, const QString &message);
    // Records a recoverable error. Unlike fail(), the block carries on with the
    // next byte and graphics mode survives; process() completes the block with
    // Completion::RecoverableError so the host is answered Cmd-9.
    void noteRecoverable(Result &result, ErrorCode code, int offset,
                         const QString &message);
    // Records a non-fatal anomaly, de-duplicated within one block.
    static void warn(Result &result, const QString &message);
    void applyPalette();
    void writePel(int x, int y, int colorIndex);
    void drawLine(int x0, int y0, int x1, int y1, bool styled = false);
    static QByteArray encodedErrorCode(ErrorCode code);
    static bool isGraphicsData(uint8_t byte);
    static int decodeCoordinate(uint8_t high, uint8_t low);
    static int decodeBufferOffset(uint8_t high, uint8_t low);

    bool m_graphicsMode = false;
    bool m_displayEnabled = false;
    bool m_suppressPacing = false;
    PendingOrder m_pendingOrder = PendingOrder::None;
    QByteArray m_pendingData;
    std::array<QColor, 8> m_palette;
    int m_colorIndex = 7;
    int m_lineWeight = 1;
    RasterFunction m_rasterFunction = RasterFunction::Replace;
    LineStyle m_style;
    StyleOffset m_styleOffset;
    FillMode m_fillMode;
    int m_marker = 0;
    ErrorCode m_lastError = ErrorCode::None;
    int m_lastErrorOffset = -1;
    QImage m_plane;
    quint64 m_blockCount = 0;
    uint8_t m_lastOrder = 0;
    QByteArray m_lastBlock;
};

} // namespace ui::rendering
