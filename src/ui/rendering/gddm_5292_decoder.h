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
#include <QPolygon>
#include <QString>
#include <QVector>
#include <array>
#include <cstdint>

namespace ui::rendering {

class Gddm5292Decoder {
  public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 288;

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
    };

    Gddm5292Decoder();

    bool recognizes(const QByteArray &data) const;
    Result process(const QByteArray &data);

    bool graphicsMode() const { return m_graphicsMode; }
    bool displayEnabled() const { return m_displayEnabled; }
    const QImage &graphicsPlane() const { return m_plane; }
    quint64 blockCount() const { return m_blockCount; }
    QByteArray statusBytes() const;

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
        Scanline,
        Polymarker,
        FillPolygon,
        ShieldArea,
        ColorTable,
        Ignore
    };

    enum class FillReference {
        Vertical,
        PolygonEdge,
        Positive45,
        Negative45
    };

    enum class FillMode {
        SolidBoundaryAndFill,
        SolidBoundaryOnly,
        FillOnly,
        StyledBoundaryOnly
    };

    struct StyleCursor {
        std::array<int, 4> lengths{};
        int segment = 0;
        int remaining = 0;

        StyleCursor(const std::array<int, 4> &style, int startSegment,
                    int overrideLength);
        bool take();
    };

    void reset(bool clearPlane);
    bool completePending(Result &result, int offset);
    bool drawPolyline(Result &result);
    bool drawScanline(Result &result);
    bool drawPolymarker(Result &result);
    bool fillPolygon(Result &result);
    bool defineShieldArea(Result &result);
    bool decodePoints(Result &result, int minimumPoints, const QString &orderName,
                      QVector<QPoint> &points);
    bool fail(Result &result, ErrorCode code, int offset, const QString &message);
    void recover(Result &result, ErrorCode code, int offset, const QString &message);
    void applyPalette();
    void writePel(int x, int y, int colorIndex);
    void drawLine(int x0, int y0, int x1, int y1, StyleCursor *style = nullptr,
                  bool skipFirst = false, bool weighted = true);
    void drawMarker(int x, int y);
    void drawPolygonBoundary(const QPolygon &polygon, bool styled);
    bool fillStyleVisible(int graphicsX, int graphicsY,
                          const QPolygon &polygon) const;
    bool isShielded(const QPoint &point) const;
    static bool pointOnPolygon(const QPoint &point, const QPolygon &polygon);
    static int nonHorizontalEdges(const QPolygon &polygon);
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
    std::array<int, 4> m_lineStyle{15, 0, 15, 0};
    int m_styleStartSegment = 0;
    int m_styleOverrideLength = 0;
    int m_marker = 0;
    FillReference m_fillReference = FillReference::Vertical;
    FillMode m_fillMode = FillMode::SolidBoundaryAndFill;
    int m_fillReferenceShift = 0;
    RasterFunction m_rasterFunction = RasterFunction::Replace;
    ErrorCode m_lastError = ErrorCode::None;
    int m_lastErrorOffset = -1;
    QVector<QPolygon> m_shieldAreas;
    int m_shieldNonHorizontalEdges = 0;
    QImage m_plane;
    quint64 m_blockCount = 0;
};

} // namespace ui::rendering
