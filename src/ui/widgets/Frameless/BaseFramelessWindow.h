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

#include "ui/widgets/Frameless/TitleBar.h"
#include <QMainWindow>
#include <QPainter>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

namespace ui::widgets {

class BaseFramelessWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit BaseFramelessWindow(QWidget *parent = nullptr);
    ~BaseFramelessWindow() override = default;

    void setWindowTitle(const QString &title);

  protected:
    TitleBar *titleBar() const { return m_titleBar; }
    QVBoxLayout *contentLayout() const { return m_contentLayout; }
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

  private:
    QWidget *m_central;
    TitleBar *m_titleBar;
    QWidget *m_content;
    QVBoxLayout *m_rootLayout;
    QVBoxLayout *m_contentLayout;

    // Drag
    bool m_dragging = false;
    QPoint m_dragOffset;

    // Edge resize (right and bottom only)
    enum Edge { None = 0, Right = 1, Bottom = 2 };
    int m_resizeEdges = None;
    bool m_resizing = false;
    QPoint m_resizeOrigin;
    QRect m_resizeGeom;
    static constexpr int ResizeMargin = 0;
    static constexpr int GripSize = 14; // triangular corner grip hit zone
    int edgesAt(const QPoint &pos) const;
    bool inCornerGrip(const QPoint &pos) const;
    Qt::CursorShape cursorForEdges(int edges) const;

    void setupUi();
    void connectControls();
    void updateResizeBorder();

  private slots:
    void onTitleMousePressed(const QPoint &globalPos);
    void onTitleMouseMoved(const QPoint &globalPos);
    void onTitleMouseReleased();
    void onTitleMouseDoubleClicked(const QPoint &globalPos);
    void onMinimize();
    void onMaximizeRestore();
    void onClose();
};

} // namespace ui::widgets
