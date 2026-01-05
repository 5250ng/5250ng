#pragma once

#include "ui/widgets/Frameless/TitleBar.h"
#include <QMainWindow>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

namespace ui::widgets {

class BaseFramelessWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit BaseFramelessWindow(QWidget *parent = nullptr);
    ~BaseFramelessWindow() override = default;

  protected:
    TitleBar *titleBar() const { return m_titleBar; }
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

  private:
    QWidget *m_central;
    TitleBar *m_titleBar;
    QWidget *m_content;
    QVBoxLayout *m_rootLayout;
    QVBoxLayout *m_contentLayout;

    // Drag
    bool m_dragging = false;
    QPoint m_dragOffset;

    void setupUi();
    void connectControls();

  private slots:
    void onTitleMousePressed(const QPoint &globalPos);
    void onTitleMouseMoved(const QPoint &globalPos);
    void onTitleMouseReleased();
    void onTitleMouseDoubleClicked(const QPoint &globalPos);
    void onMinimize();
    void onMaximizeRestore();
    void onClose();
};

} // namespace ui::widgets


