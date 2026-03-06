#include "QConnectionStatusWidget.h"
#include <QFontMetrics>
#include <QtWidgets/QStyle>

namespace ui::widgets {

QConnectionStatusWidget::QConnectionStatusWidget(QWidget *parent)
    : QWidget(parent),
      m_dot(new QLabel(this)),
      m_text(new QLabel(this)),
      m_layout(new QHBoxLayout(this)),
      m_state(tn5250::client::TN5250Client::ConnectionState::Disconnected) {
    m_dot->setObjectName("StatusIndicator");
    m_dot->setFixedSize(12, 12);

    m_text->setText("Not connected");
    // Ensure text is visible on dark/black footers
    m_text->setStyleSheet("color: white; background: transparent;");

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);
    m_layout->addWidget(m_dot, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_text, 0, Qt::AlignVCenter);
    setLayout(m_layout);

    updateVisuals();
}

void QConnectionStatusWidget::setState(tn5250::client::TN5250Client::ConnectionState state) {
    if (m_state == state) {
        return;
    }
    m_state = state;
    updateVisuals();
}

void QConnectionStatusWidget::setStatusText(const QString &text) {
    m_text->setText(text);
}

QString QConnectionStatusWidget::statusText() const {
    return m_text->text();
}

void QConnectionStatusWidget::setTextColor(const QColor &color) {
    m_text->setStyleSheet(
        QString("color: %1; background: transparent;").arg(color.name(QColor::HexRgb)));
}

void QConnectionStatusWidget::updateVisuals() {
    // Set dynamic property used by themes to style the dot
    // Values: "disconnected", "connecting", "negotiating", "connected", "error"
    QString stateProp;
    QString tooltip;
    QString text = m_text->text();
    switch (m_state) {
    case tn5250::client::TN5250Client::ConnectionState::Disconnected:
        stateProp = "disconnected";
        tooltip = "Not connected";
        if (text.isEmpty() || text == "Ready")
            text = "Not connected";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Connecting:
        stateProp = "connecting";
        tooltip = "Connecting";
        if (text.isEmpty())
            text = "Connecting";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Negotiating:
        stateProp = "negotiating";
        tooltip = "Waiting for system";
        if (text.isEmpty())
            text = "Waiting for system";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Connected:
        stateProp = "connected";
        tooltip = "Ready";
        if (text.isEmpty() || text == "Not connected")
            text = "Ready";
        break;
    case tn5250::client::TN5250Client::ConnectionState::Error:
        stateProp = "error";
        tooltip = "Error";
        if (text.isEmpty())
            text = "Error";
        break;
    }
    m_dot->setProperty("state", stateProp);
    m_dot->setToolTip(tooltip);
    m_text->setText(text);
    // Re-apply stylesheet so dynamic property takes effect
    style()->unpolish(m_dot);
    style()->polish(m_dot);
    m_dot->update();
}

} // namespace ui::widgets
