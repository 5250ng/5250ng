#pragma once

#include "network/tn5250/client/client.h"
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace ui::widgets {

/**
 * Small status widget showing a colored dot and a text label.
 * The dot is styled via stylesheet using objectName "StatusIndicator"
 * and a dynamic property "state" updated by setState(...).
 */
class QConnectionStatusWidget : public QWidget {
    Q_OBJECT
  public:
    explicit QConnectionStatusWidget(QWidget *parent = nullptr);
    ~QConnectionStatusWidget() override = default;

    void setState(tn5250::client::TN5250Client::ConnectionState state);
    tn5250::client::TN5250Client::ConnectionState state() const { return m_state; }

    void setStatusText(const QString &text);
    QString statusText() const;
    void setTextColor(const QColor &color);

  private:
    QLabel *m_dot;
    QLabel *m_text;
    QHBoxLayout *m_layout;
    tn5250::client::TN5250Client::ConnectionState m_state;

    void updateVisuals();
};

} // namespace ui::widgets
