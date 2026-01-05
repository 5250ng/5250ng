#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include "Q5250ScreenWidget.h"
#include "Q5250HRule.h"

namespace ui::widgets {

class Q5250TerminalView : public QWidget {
    Q_OBJECT
  public:
    explicit Q5250TerminalView(QWidget *parent = nullptr);
    ~Q5250TerminalView() override = default;

    Q5250ScreenWidget *screen() const { return m_screen; }
    Q5250ScreenWidget *footer() const { return m_footer; }
    Q5250HRule *rule() const { return m_rule; }

    void setScreenSize(int rows, int cols);
    void setFont(const QFont &font);

  private slots:
    void onScreenSizeChanged(int rows, int cols);

  private:
    QVBoxLayout *m_layout;
    Q5250ScreenWidget *m_screen;
    Q5250HRule *m_rule;
    Q5250ScreenWidget *m_footer;
};

} // namespace ui::widgets


