#pragma once

#include <QWidget>

namespace ui::widgets {

class Q5250HRule : public QWidget {
    Q_OBJECT
  public:
    explicit Q5250HRule(QWidget *parent = nullptr);
    ~Q5250HRule() override = default;

  protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

} // namespace ui::widgets


