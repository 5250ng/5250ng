#pragma once

#include <QColor>
#include <QWidget>

namespace ui::widgets {

class Q5250Cursor : public QWidget {
    Q_OBJECT
  public:
    explicit Q5250Cursor(QWidget *parent = nullptr);
    ~Q5250Cursor() override = default;

    void setColor(const QColor &c);
    QColor color() const { return m_color; }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QColor m_color;
};

} // namespace ui::widgets


