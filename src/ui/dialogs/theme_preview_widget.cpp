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

#include "theme_preview_widget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>

// Preview renders a mini 5250 sign-on screen using all protocol colors.
// Grid: 14 rows x 50 columns (compact preview).

static constexpr int kPreviewRows = 14;
static constexpr int kPreviewCols = 50;

ThemePreviewWidget::ThemePreviewWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(minimumSizeHint());
    buildPreviewScreen();
}

void ThemePreviewWidget::setTheme(const ui::themes::TerminalTheme &theme) {
    m_theme = theme;
    m_colorScheme = theme.buildColorScheme();
    update();
}

void ThemePreviewWidget::buildPreviewScreen() {
    // Initialize empty screen
    m_previewScreen.resize(kPreviewRows);
    for (int r = 0; r < kPreviewRows; ++r) {
        m_previewScreen[r].resize(kPreviewCols);
        for (int c = 0; c < kPreviewCols; ++c) {
            m_previewScreen[r][c] = {' ', 2, false, false}; // Green spaces
        }
    }

    // Sign On screen mockup using various 5250 colors
    // Color indices: 2=green, 9=blue, 11=cyan, 12=red, 13=pink, 14=yellow, 15=white

    addText(0, 18, "Sign On", 15, false, false);

    addText(2, 2, "System  . . . . . :", 2);
    addText(2, 23, "S1051PC3", 11);

    addText(3, 2, "Subsystem . . . . :", 2);
    addText(3, 23, "QINTER", 11);

    addText(4, 2, "Display . . . . . :", 2);
    addText(4, 23, "QPADEV0001", 11);

    addText(6, 2, "User  . . . . . . :", 2);
    addText(6, 23, "________", 2, false, true);

    addText(7, 2, "Password  . . . . :", 2);
    addText(7, 23, "        ", 2, true, false);

    addText(9, 2, "Program/procedure :", 2);
    addText(9, 23, "________", 14);

    addText(11, 2, "F3=Exit", 9);
    addText(11, 12, "F5=Refresh", 9);
    addText(11, 25, "F12=Disconnect", 9);

    addText(13, 2, "(C) COPYRIGHT IBM CORP.", 12);
    addText(13, 28, "5250ng v0.5", 13);
}

void ThemePreviewWidget::addText(int row, int col, const QString &text,
                                  int colorIndex, bool reverse, bool underline) {
    if (row < 0 || row >= kPreviewRows || col < 0) return;
    for (int i = 0; i < text.length() && (col + i) < kPreviewCols; ++i) {
        m_previewScreen[row][col + i] = {text[i], colorIndex, reverse, underline};
    }
}

void ThemePreviewWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Calculate cell size to fill the widget
    int cellW = width() / kPreviewCols;
    int cellH = height() / kPreviewRows;
    if (cellW < 1) cellW = 1;
    if (cellH < 1) cellH = 1;

    // Center the grid
    int totalW = cellW * kPreviewCols;
    int totalH = cellH * kPreviewRows;
    int offsetX = (width() - totalW) / 2;
    int offsetY = (height() - totalH) / 2;

    QRect gridArea(offsetX, offsetY, totalW, totalH);

    QColor bgColor = m_colorScheme.isEmpty() ? QColor(0, 0, 0)
                                              : m_theme.adjustColor(m_theme.backgroundColor);

    // Render all pre-CRT content to an offscreen image so the phosphor bloom
    // effect can read from it without calling grab() (which causes recursive repaint).
    QImage offscreen(size(), QImage::Format_ARGB32_Premultiplied);
    offscreen.fill(Qt::transparent);
    {
        QPainter op(&offscreen);
        op.setRenderHint(QPainter::TextAntialiasing);

        // Fill background
        op.fillRect(rect(), bgColor);

        // Set font
        QFont font(m_theme.fontFamily.isEmpty() ? "Courier" : m_theme.fontFamily);
        font.setPixelSize(qMax(cellH - 2, 8));
        op.setFont(font);

        // Draw cells
        for (int row = 0; row < kPreviewRows; ++row) {
            for (int col = 0; col < kPreviewCols; ++col) {
                const PreviewCell &cell = m_previewScreen[row][col];
                QRect cellRect(offsetX + col * cellW, offsetY + row * cellH, cellW, cellH);

                QColor fg = (cell.colorIndex < m_colorScheme.size())
                                ? m_colorScheme[cell.colorIndex]
                                : QColor(0, 255, 0);
                QColor bg = bgColor;

                if (cell.reverse) {
                    qSwap(fg, bg);
                }

                op.fillRect(cellRect, bg);
                op.setPen(fg);
                op.drawText(cellRect, Qt::AlignCenter, cell.ch);

                if (cell.underline) {
                    op.drawLine(cellRect.bottomLeft(), cellRect.bottomRight());
                }
            }
        }

        // Draw column separators (every 10 columns, like real 5250 column indicators)
        if (m_theme.columnSeparatorEnabled) {
            QColor sepColor = m_theme.adjustColor(m_theme.columnSeparatorColor.isValid()
                                  ? m_theme.columnSeparatorColor : QColor(128, 128, 128));
            QPen sepPen(sepColor, 1);
            if (m_theme.columnSeparatorStyle == ui::themes::TerminalTheme::Dotted) {
                sepPen.setStyle(Qt::DotLine);
            } else if (m_theme.columnSeparatorStyle == ui::themes::TerminalTheme::Dimmed) {
                QColor dimmed = sepColor;
                dimmed.setAlpha(dimmed.alpha() / 3);
                sepPen.setColor(dimmed);
            }
            op.setPen(sepPen);
            for (int c = 10; c < kPreviewCols; c += 10) {
                int x = offsetX + c * cellW;
                op.drawLine(x, offsetY, x, offsetY + totalH);
            }
        }

        // Draw selection highlight on "S1051PC3" (row 2, cols 23-30) to show selection colors.
        // Matches the real 5250 screen: translucent BG overlay, FG text override, themed border.
        {
            const int selRow = 2;
            const int selColStart = 23;
            const int selColEnd = qMin(30, kPreviewCols - 1); // inclusive
            QColor selBg = m_theme.adjustColor(m_theme.selectionBackground.isValid()
                               ? m_theme.selectionBackground : QColor(255, 255, 0, 64));
            QColor selFg = m_theme.adjustColor(m_theme.selectionForeground.isValid()
                               ? m_theme.selectionForeground : QColor(255, 255, 255));
            QColor selBorder = m_theme.adjustColor(m_theme.selectionBorder.isValid()
                               ? m_theme.selectionBorder : QColor(255, 255, 0));

            // Overlay fill + redraw text with selection FG (when FG is visible)
            for (int c = selColStart; c <= selColEnd; ++c) {
                QRect selRect(offsetX + c * cellW, offsetY + selRow * cellH, cellW, cellH);
                op.fillRect(selRect, selBg);
                if (selFg.alpha() > 0) {
                    const PreviewCell &cell = m_previewScreen[selRow][c];
                    op.setPen(selFg);
                    op.drawText(selRect, Qt::AlignCenter, cell.ch);
                }
            }

            // Themed 2px border around the selection rectangle
            QRect borderRect(offsetX + selColStart * cellW,
                             offsetY + selRow * cellH,
                             (selColEnd - selColStart + 1) * cellW,
                             cellH);
            QPen borderPen(selBorder, 2);
            op.setPen(borderPen);
            op.setBrush(Qt::NoBrush);
            op.drawRect(borderRect.adjusted(1, 1, -1, -1));
        }

        // Draw cursor at the User input field position
        QRect cursorCellRect(offsetX + kCursorCol * cellW, offsetY + kCursorRow * cellH,
                             cellW, cellH);
        renderCursor(op, cursorCellRect);
    }

    // Blit the offscreen image to the widget
    QPainter painter(this);
    painter.drawImage(0, 0, offscreen);

    // Apply CRT effects if enabled (reads from offscreen image for bloom)
    if (m_theme.crtEffectEnabled) {
        renderCRTEffect(painter, gridArea, offscreen);
    }

    // Draw border around the preview
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawRect(offsetX, offsetY, totalW - 1, totalH - 1);
}

void ThemePreviewWidget::renderCursor(QPainter &painter, const QRect &cellRect) {
    QColor cursorColor = m_theme.adjustColor(m_theme.cursorColor.isValid()
                                                        ? m_theme.cursorColor
                                                        : QColor(0, 255, 0));
    switch (m_theme.cursorShape) {
    case ui::themes::TerminalTheme::Block:
        painter.fillRect(cellRect, cursorColor);
        break;
    case ui::themes::TerminalTheme::Underline: {
        int h = qMax(2, cellRect.height() / 6);
        QRect underline(cellRect.x(), cellRect.bottom() - h + 1, cellRect.width(), h);
        painter.fillRect(underline, cursorColor);
        break;
    }
    case ui::themes::TerminalTheme::Bar: {
        int w = qMax(2, cellRect.width() / 6);
        QRect bar(cellRect.x(), cellRect.y(), w, cellRect.height());
        painter.fillRect(bar, cursorColor);
        break;
    }
    }
}

void ThemePreviewWidget::renderCRTEffect(QPainter &painter, const QRect &area,
                                          const QImage &preRendered) {
    // Phosphor bloom: local glow around characters via downscale blur
    // Uses the pre-rendered offscreen image instead of grab() to avoid recursive repaint.
    if (m_theme.crtPhosphorBloom > 0.01) {
        QImage snapshot = preRendered.copy(area);
        if (!snapshot.isNull()) {
            int w = snapshot.width();
            int h = snapshot.height();
            QImage blurred = snapshot;
            for (int pass = 0; pass < 2; ++pass) {
                w = qMax(1, w / 4);
                h = qMax(1, h / 4);
                blurred = blurred.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            blurred = blurred.scaled(snapshot.width(), snapshot.height(),
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            painter.save();
            painter.setCompositionMode(QPainter::CompositionMode_Plus);
            painter.setOpacity(m_theme.crtPhosphorBloom * 0.7);
            painter.drawImage(area.topLeft(), blurred);
            painter.restore();
        }
    }

    // Scanlines: alternating semi-transparent dark horizontal lines
    if (m_theme.crtScanlineIntensity > 0.01) {
        QColor scanColor(0, 0, 0, static_cast<int>(m_theme.crtScanlineIntensity * 180));
        for (int y = area.top(); y < area.bottom(); y += 2) {
            painter.fillRect(area.left(), y, area.width(), 1, scanColor);
        }
    }

    // Phosphor glow: radial gradient, composited additively
    if (m_theme.crtGlowRadius > 0.01) {
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        QRadialGradient glow(area.center(), area.width() * 0.6);
        QColor glowColor = m_theme.adjustColor(m_theme.colorGreen.isValid()
                                                         ? m_theme.colorGreen
                                                         : QColor(0, 255, 0));
        glowColor.setAlphaF(m_theme.crtGlowRadius * 0.15);
        glow.setColorAt(0.0, glowColor);
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(area, glow);
        painter.restore();
    }

    // Curvature vignette: darken edges
    if (m_theme.crtCurvature > 0.01) {
        QRadialGradient vignette(area.center(), area.width() * 0.7);
        vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
        vignette.setColorAt(0.8, QColor(0, 0, 0, 0));
        vignette.setColorAt(1.0, QColor(0, 0, 0, static_cast<int>(m_theme.crtCurvature * 200)));
        painter.fillRect(area, vignette);
    }
}
