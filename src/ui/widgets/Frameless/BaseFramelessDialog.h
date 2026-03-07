#pragma once

#include "ui/widgets/Frameless/TitleBar.h"
#include <QDialog>
#include <QVBoxLayout>

namespace ui::widgets {

class BaseFramelessDialog : public QDialog {
    Q_OBJECT
  public:
    explicit BaseFramelessDialog(QWidget *parent = nullptr);
    ~BaseFramelessDialog() override = default;

    void setWindowTitle(const QString &title);

  protected:
    TitleBar *titleBar() const { return m_titleBar; }
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

  private:
    TitleBar *m_titleBar;
    QVBoxLayout *m_contentLayout;

    // Drag
    bool m_dragging = false;
    QPoint m_dragOffset;

  private slots:
    void onTitleMousePressed(const QPoint &globalPos);
    void onTitleMouseMoved(const QPoint &globalPos);
    void onTitleMouseReleased();
};

} // namespace ui::widgets
