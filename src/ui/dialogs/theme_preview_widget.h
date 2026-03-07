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

#pragma once

#include "ui/themes/terminal_theme.h"
#include <QWidget>

class ThemePreviewWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ThemePreviewWidget(QWidget *parent = nullptr);

    void setTheme(const ui::themes::TerminalTheme &theme);

    QSize sizeHint() const override { return QSize(480, 220); }
    QSize minimumSizeHint() const override { return QSize(320, 150); }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    ui::themes::TerminalTheme m_theme;
    QVector<QColor> m_colorScheme;

    struct PreviewCell {
        QChar ch;
        int colorIndex;  // index into 16-color scheme
        bool reverse = false;
        bool underline = false;
    };

    QVector<QVector<PreviewCell>> m_previewScreen;
    void buildPreviewScreen();
    void addText(int row, int col, const QString &text, int colorIndex,
                 bool reverse = false, bool underline = false);

    // CRT post-processing
    void renderCRTEffect(QPainter &painter, const QRect &area,
                         const QImage &preRendered);
    // Cursor preview
    void renderCursor(QPainter &painter, const QRect &cellRect);

    // Cursor position in the preview (on the User input field)
    static constexpr int kCursorRow = 6;
    static constexpr int kCursorCol = 23;
};
