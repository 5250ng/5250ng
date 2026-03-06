#pragma once

#include <QColor>
#include <QWidget>

namespace ui::widgets {

class Q5250HRule : public QWidget {
    Q_OBJECT
  public:
    explicit Q5250HRule(QWidget *parent = nullptr);
    ~Q5250HRule() override = default;

    void setColor(const QColor &color);
    QColor color() const { return m_color; }

  protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  private:
    QColor m_color;
};

} // namespace ui::widgets


