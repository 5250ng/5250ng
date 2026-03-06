#include "Q5250HRule.h"
#include <QPainter>

namespace ui::widgets {

Q5250HRule::Q5250HRule(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(2);
    m_color = palette().color(QPalette::Mid);
}

void Q5250HRule::setColor(const QColor &color) {
    if (m_color == color)
        return;
    m_color = color;
    update();
}

void Q5250HRule::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), m_color);
}

QSize Q5250HRule::sizeHint() const {
    return QSize(100, 2);
}

QSize Q5250HRule::minimumSizeHint() const {
    return QSize(10, 2);
}

} // namespace ui::widgets
