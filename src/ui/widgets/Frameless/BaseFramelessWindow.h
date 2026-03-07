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

    void setWindowTitle(const QString &title);

  protected:
    TitleBar *titleBar() const { return m_titleBar; }
    QVBoxLayout *contentLayout() const { return m_contentLayout; }
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

  private:
    QWidget *m_central;
    TitleBar *m_titleBar;
    QWidget *m_content;
    QVBoxLayout *m_rootLayout;
    QVBoxLayout *m_contentLayout;

    // Drag
    bool m_dragging = false;
    QPoint m_dragOffset;

    // Edge resize
    enum Edge { None = 0, Left = 1, Top = 2, Right = 4, Bottom = 8 };
    int m_resizeEdges = None;
    bool m_resizing = false;
    QPoint m_resizeOrigin;
    QRect m_resizeGeom;
    static constexpr int ResizeMargin = 6;
    int edgesAt(const QPoint &pos) const;
    Qt::CursorShape cursorForEdges(int edges) const;

    void setupUi();
    void connectControls();
    void updateResizeBorder();

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
