#pragma once

#include "ui/themes/terminal_theme.h"
#include <QColor>
#include <QWidget>

namespace ui::widgets {

class Q5250Cursor : public QWidget {
    Q_OBJECT
  public:
    using CursorShape = ui::themes::TerminalTheme::CursorShape;

    explicit Q5250Cursor(QWidget *parent = nullptr);
    ~Q5250Cursor() override = default;

    void setColor(const QColor &c);
    QColor color() const { return m_color; }

    void setCursorShape(CursorShape shape);
    CursorShape cursorShape() const { return m_shape; }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QColor m_color;
    CursorShape m_shape = CursorShape::Block;
};

} // namespace ui::widgets


