#pragma once

#include <QColor>
#include <QImage>
#include <QWidget>

namespace ui::widgets {

// Transparent overlay widget that renders CRT post-processing effects
// (scanlines, phosphor bloom, glow, curvature vignette) over the entire parent area.
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
    void setPhosphorBloom(double bloom);

    // Call when underlying content changes so bloom cache is invalidated
    void invalidateBloom();

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    QImage grabAndBlur(int passes, int downscale);

    bool m_enabled = false;
    double m_scanlineIntensity = 0.0;
    double m_glowRadius = 0.0;
    double m_curvature = 0.0;
    double m_phosphorBloom = 0.0;
    QColor m_glowColor = QColor(0, 255, 0);

    // Bloom cache
    QImage m_bloomCache;
    bool m_bloomDirty = true;
    bool m_painting = false; // recursion guard for grab
};

} // namespace ui::widgets
