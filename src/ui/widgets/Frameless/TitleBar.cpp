#include "TitleBar.h"
#include <QAction>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include "ui/themes/manager.h"

namespace ui::widgets {

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent),
      m_menuBar(new QMenuBar(this)),
      m_minButton(new QPushButton("_", this)),
      m_maxButton(new QPushButton("□", this)),
      m_closeButton(new QPushButton("✕", this)),
      m_layout(new QHBoxLayout(this)) {
    m_layout->setContentsMargins(8, 4, 8, 4);
    m_layout->setSpacing(6);
    m_menuBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_layout->addWidget(m_menuBar, 0);
    // Centered bold title label (overlay, not part of layout)
    m_titleLabel = new QLabel("5250ng", this);
    QFont f = m_titleLabel->font();
    f.setBold(true);
    m_titleLabel->setFont(f);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_titleLabel->raise();
    m_layout->addStretch(1);
    m_minButton->setFixedSize(20, 20);
    m_maxButton->setFixedSize(20, 20);
    m_closeButton->setFixedSize(20, 20);
    m_minButton->setFlat(true);
    m_maxButton->setFlat(true);
    m_closeButton->setFlat(true);
    m_layout->addWidget(m_minButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_maxButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    setLayout(m_layout);

    // Remove menubar bottom border (if any, depending on style)
    m_menuBar->setStyleSheet("QMenuBar { border: none; }");

    connect(m_minButton, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maxButton, &QPushButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(m_closeButton, &QPushButton::clicked, this, &TitleBar::closeRequested);

    // Install event filter only on the menubar to allow drag on empty areas
    // m_menuBar->installEventFilter(this);

    // Bottom hairline across entire title bar
    m_bottomLine = new QFrame(this);
    m_bottomLine->setFrameShape(QFrame::NoFrame);
    m_bottomLine->setFixedHeight(1);
    {
        // Use theme color if available; fallback to palette(mid)
        const QString c = ui::themes::ThemeManager::instance().color("titlebar.hline");
        if (!c.isEmpty()) {
            m_bottomLine->setStyleSheet(QString("background-color: %1;").arg(c));
        } else {
            m_bottomLine->setStyleSheet("background-color: palette(mid);");
        }
    }
    m_bottomLine->raise();
}

bool TitleBar::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_menuBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                // Only start drag if no action under cursor
                QAction *act = m_menuBar->actionAt(me->position().toPoint());
                if (act != nullptr) {
                    return QWidget::eventFilter(obj, event);
                }
                emit mousePressed(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            emit mouseMoved(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            emit mouseReleased();
            return true;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            // Ignore dblclick over actions
            QAction *act = m_menuBar->actionAt(me->position().toPoint());
            if (act != nullptr) {
                return QWidget::eventFilter(obj, event);
            }
            emit mouseDoubleClicked(me->globalPosition().toPoint());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TitleBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_titleLabel) {
        m_titleLabel->setGeometry(rect());
    }
    if (m_bottomLine) {
        m_bottomLine->setGeometry(0, height() - 1, width(), 1);
    }
}

void TitleBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit mousePressed(event->globalPosition().toPoint());
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event) {
    emit mouseMoved(event->globalPosition().toPoint());
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event) {
    emit mouseReleased();
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
    emit mouseDoubleClicked(event->globalPosition().toPoint());
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace ui::widgets
