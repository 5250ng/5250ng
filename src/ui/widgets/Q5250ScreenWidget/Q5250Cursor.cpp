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

void Q5250Cursor::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), m_color);
}

} // namespace ui::widgets


