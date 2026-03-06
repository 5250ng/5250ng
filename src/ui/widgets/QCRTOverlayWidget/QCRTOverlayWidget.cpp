#include "QCRTOverlayWidget.h"
#include <QPainter>
#include <QRadialGradient>

namespace ui::widgets {

QCRTOverlayWidget::QCRTOverlayWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
}

void QCRTOverlayWidget::setEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    setVisible(m_enabled);
    m_bloomDirty = true;
    update();
}

void QCRTOverlayWidget::setScanlineIntensity(double intensity) {
    m_scanlineIntensity = intensity;
    update();
}

void QCRTOverlayWidget::setGlowRadius(double radius) {
    m_glowRadius = radius;
    update();
}

void QCRTOverlayWidget::setGlowColor(const QColor &color) {
    m_glowColor = color;
    update();
}

void QCRTOverlayWidget::setCurvature(double curvature) {
    m_curvature = curvature;
    update();
}

void QCRTOverlayWidget::setPhosphorBloom(double bloom) {
    m_phosphorBloom = bloom;
    m_bloomDirty = true;
    update();
}

void QCRTOverlayWidget::invalidateBloom() {
    m_bloomDirty = true;
}

// Grab parent content (excluding this overlay) and produce a blurred image.
// Uses multi-pass downscale/upscale for a fast gaussian-like blur.
QImage QCRTOverlayWidget::grabAndBlur(int passes, int downscale) {
    QWidget *p = parentWidget();
    if (!p) return {};

    // Hide overlay, grab parent, restore
    m_painting = true;
    hide();
    QPixmap content = p->grab();
    show();
    raise();
    m_painting = false;

    QImage img = content.toImage();
    if (img.isNull()) return {};

    // Multi-pass downscale for gaussian-like blur
    int w = img.width();
    int h = img.height();
    QImage result = img;
    for (int i = 0; i < passes; ++i) {
        w = qMax(1, w / downscale);
        h = qMax(1, h / downscale);
        result = result.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    // Scale back to original size
    result = result.scaled(img.width(), img.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return result;
}

void QCRTOverlayWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (!m_enabled || m_painting) return;

    QPainter painter(this);

    // 1. Phosphor bloom: local glow around each lit character
    if (m_phosphorBloom > 0.01) {
        // Regenerate bloom cache when dirty
        if (m_bloomDirty || m_bloomCache.isNull()
            || m_bloomCache.size() != size()) {
            // 2 downscale passes at 4x each = 16x total reduction, very soft bloom
            m_bloomCache = grabAndBlur(2, 4);
            m_bloomDirty = false;
        }
        if (!m_bloomCache.isNull()) {
            painter.save();
            painter.setCompositionMode(QPainter::CompositionMode_Plus);
            painter.setOpacity(m_phosphorBloom * 0.7);
            painter.drawImage(0, 0, m_bloomCache);
            painter.restore();
        }
    }

    // 2. Scanlines: alternating semi-transparent dark horizontal lines
    if (m_scanlineIntensity > 0.01) {
        int alpha = static_cast<int>(m_scanlineIntensity * 180);
        QColor scanlineColor(0, 0, 0, alpha);
        painter.setPen(Qt::NoPen);
        painter.setBrush(scanlineColor);
        for (int y = 0; y < height(); y += 2) {
            painter.drawRect(0, y, width(), 1);
        }
    }

    // 3. Global phosphor glow: subtle tinted glow overlay with gradient from center
    if (m_glowRadius > 0.01) {
        int glowAlpha = static_cast<int>(m_glowRadius * 40);
        QColor glowColor(m_glowColor.red(), m_glowColor.green(), m_glowColor.blue(), glowAlpha);
        QRadialGradient gradient(rect().center(), qMax(width(), height()) * 0.7);
        gradient.setColorAt(0.0, glowColor);
        gradient.setColorAt(1.0, Qt::transparent);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        painter.drawRect(rect());
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // 4. Curvature: vignette darkening at edges to simulate CRT screen curvature
    if (m_curvature > 0.01) {
        int vignetteAlpha = static_cast<int>(m_curvature * 255);
        QRadialGradient vignette(rect().center(), qMax(width(), height()) * 0.6);
        vignette.setColorAt(0.0, Qt::transparent);
        vignette.setColorAt(0.7, Qt::transparent);
        vignette.setColorAt(1.0, QColor(0, 0, 0, vignetteAlpha));
        painter.setPen(Qt::NoPen);
        painter.setBrush(vignette);
        painter.drawRect(rect());
    }
}

} // namespace ui::widgets
