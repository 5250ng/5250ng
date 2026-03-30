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
    void changeEvent(QEvent *event) override;

  private:
    QWidget *m_central;
    TitleBar *m_titleBar;
    QWidget *m_content;
    QVBoxLayout *m_rootLayout;
    QVBoxLayout *m_contentLayout;

    // Edge resize detection (all 4 edges + 4 corners)
    static constexpr int ResizeMargin = 5;
    Qt::Edges edgesAt(const QPoint &pos) const;
    Qt::CursorShape cursorForEdges(Qt::Edges edges) const;

    void setupUi();
    void connectControls();
    void updateResizeBorder();

    // Fallback drag for platforms without startSystemMove()
    bool m_fallbackDragging = false;
    QPoint m_fallbackDragOffset;

  private slots:
    void onTitleMousePressed(const QPoint &globalPos);
    void onTitleMouseDoubleClicked(const QPoint &globalPos);
    void onMinimize();
    void onMaximizeRestore();
    void onClose();
};

} // namespace ui::widgets
