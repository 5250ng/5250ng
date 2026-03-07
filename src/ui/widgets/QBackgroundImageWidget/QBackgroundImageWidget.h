#pragma once

#include "ui/themes/terminal_theme.h"
#include <QPixmap>
#include <QWidget>

namespace ui::widgets {

// Widget that renders a background image behind all sibling widgets.
// Must be lowered below siblings and resized to match the parent.
class QBackgroundImageWidget : public QWidget {
    Q_OBJECT
  public:
    explicit QBackgroundImageWidget(QWidget *parent = nullptr);

    void setImage(const QPixmap &pixmap);
    void setImage(const QByteArray &data);
    void clearImage();
    bool hasImage() const { return !m_image.isNull(); }

    void setLayout(ui::themes::TerminalTheme::BackgroundImageLayout layout);
    void setImageOpacity(double opacity);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QPixmap m_image;
    ui::themes::TerminalTheme::BackgroundImageLayout m_layout =
        ui::themes::TerminalTheme::Stretch;
    double m_opacity = 1.0;
};

} // namespace ui::widgets
