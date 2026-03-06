#pragma once

#include <QColor>
#include <QWidget>

namespace ui::widgets {

// Transparent overlay widget that renders CRT post-processing effects
// (scanlines, phosphor glow, curvature vignette) over the entire parent area.
// Must be raised above siblings and resized to match the parent.
class QCRTOverlayWidget : public QWidget {
    Q_OBJECT
  public:
    explicit QCRTOverlayWidget(QWidget *parent = nullptr);

    void setEnabled(bool enabled);
    void setScanlineIntensity(double intensity);
    void setGlowRadius(double radius);
    void setGlowColor(const QColor &color);
    void setCurvature(double curvature);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    bool m_enabled = false;
    double m_scanlineIntensity = 0.0;
    double m_glowRadius = 0.0;
    double m_curvature = 0.0;
    QColor m_glowColor = QColor(0, 255, 0);
};

} // namespace ui::widgets
