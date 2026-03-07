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
#include <QPixmap>
#include <QWidget>

namespace ui::widgets {

// Widget that renders a background image behind all sibling widgets.
// Must be lowered below siblings and resized to match the parent.
class QBackgroundImageWidget : public QWidget {
    Q_OBJECT
  public:
    explicit QBackgroundImageWidget(QWidget *parent = nullptr);

    void setImage(const QPixmap &pixmap);
    void setImage(const QByteArray &data);
    void clearImage();
    bool hasImage() const { return !m_image.isNull(); }

    void setLayout(ui::themes::TerminalTheme::BackgroundImageLayout layout);
    void setImageOpacity(double opacity);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QPixmap m_image;
    ui::themes::TerminalTheme::BackgroundImageLayout m_layout =
        ui::themes::TerminalTheme::Stretch;
    double m_opacity = 1.0;
};

} // namespace ui::widgets
