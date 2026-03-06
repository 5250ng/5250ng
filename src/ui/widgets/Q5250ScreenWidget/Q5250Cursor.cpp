#include "Q5250Cursor.h"
#include <QPainter>

namespace ui::widgets {

Q5250Cursor::Q5250Cursor(QWidget *parent) : QWidget(parent), m_color(Qt::green) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setFocusPolicy(Qt::NoFocus);
}

void Q5250Cursor::setColor(const QColor &c) {
    if (m_color == c)
        return;
    m_color = c;
    update();
}

void Q5250Cursor::setCursorShape(CursorShape shape) {
    if (m_shape == shape)
        return;
    m_shape = shape;
    update();
}

void Q5250Cursor::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);

    switch (m_shape) {
    case CursorShape::Block:
        p.fillRect(rect(), m_color);
        break;
    case CursorShape::Underline: {
        // Draw a 2px line at the bottom of the cell
        int h = qMax(2, rect().height() / 6);
        QRect underline(0, rect().height() - h, rect().width(), h);
        p.fillRect(underline, m_color);
        break;
    }
    case CursorShape::Bar: {
        // Draw a 2px vertical line on the left edge
        int w = qMax(2, rect().width() / 6);
        QRect bar(0, 0, w, rect().height());
        p.fillRect(bar, m_color);
        break;
    }
    }
}

} // namespace ui::widgets
