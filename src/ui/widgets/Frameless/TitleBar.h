#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QWidget>

namespace ui::widgets {

class TitleBar : public QWidget {
    Q_OBJECT
  public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override = default;

    QMenuBar *menuBar() const { return m_menuBar; }
    void setTitle(const QString &title);
    void setMinMaxVisible(bool visible);

  signals:
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();
    void mousePressed(const QPoint &globalPos);
    void mouseMoved(const QPoint &globalPos);
    void mouseReleased();
    void mouseDoubleClicked(const QPoint &globalPos);

  private slots:
    void applyTheme();

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    QMenuBar *m_menuBar;
    QLabel *m_titleLabel;
    class QFrame *m_bottomLine;
    QPushButton *m_minButton;
    QPushButton *m_maxButton;
    QPushButton *m_closeButton;
    QHBoxLayout *m_layout;
};

} // namespace ui::widgets
