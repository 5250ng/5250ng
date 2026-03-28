// 5250ng - A modern IBM TN5250 terminal emulator                                                                                                                                                            
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)                                                                                                                                                          
//                                                                                                                                                                                                           
// This program is free software: you can redistribute it and/or modify                                                                                                                                      
// it under the terms of the GNU General Public License as published by                                                                                                                                      
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.                                                                                                                                                                       
//                                                                                                                                                                                                           
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "TitleBar.h"
#include <QAction>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QIcon>
#include "ui/themes/manager.h"

namespace ui::widgets {

// macOS traffic-light colors
static const char *kMinimizeBg   = "#febc2e";  // yellow/orange
static const char *kMaximizeBg   = "#28c840";  // green
static const char *kCloseBg      = "#ff5f57";  // red

static const char *kMinimizeHover = "#e5a820";
static const char *kMaximizeHover = "#1eaf32";
static const char *kCloseHover    = "#e5453b";

static QString buttonStyle(const char *bg, const char *hover) {
    return QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 0px;"
        "  qproperty-iconSize: 8px 8px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "}"
    ).arg(bg, hover);
}

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent),
      m_menuBar(new QMenuBar(this)),
      m_minButton(new QPushButton(this)),
      m_maxButton(new QPushButton(this)),
      m_closeButton(new QPushButton(this)),
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

    // Configure traffic-light buttons (right side: minimize, maximize, close)
    const int btnSize = 14;
    const int iconSize = 8;
    for (auto *btn : {m_minButton, m_maxButton, m_closeButton}) {
        btn->setFixedSize(btnSize, btnSize);
        btn->setFlat(true);
        btn->setIconSize(QSize(iconSize, iconSize));
        btn->setCursor(Qt::ArrowCursor);
    }

    m_minButton->setIcon(QIcon(":/titlebar/icons/minimize.svg"));
    m_maxButton->setIcon(QIcon(":/titlebar/icons/maximize.svg"));
    m_closeButton->setIcon(QIcon(":/titlebar/icons/close.svg"));

    m_minButton->setStyleSheet(buttonStyle(kMinimizeBg, kMinimizeHover));
    m_maxButton->setStyleSheet(buttonStyle(kMaximizeBg, kMaximizeHover));
    m_closeButton->setStyleSheet(buttonStyle(kCloseBg, kCloseHover));

    m_layout->addWidget(m_minButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_maxButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    setLayout(m_layout);

    connect(m_minButton, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maxButton, &QPushButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(m_closeButton, &QPushButton::clicked, this, &TitleBar::closeRequested);

    // Install event filter on the menubar to allow drag on empty areas
    m_menuBar->installEventFilter(this);

    // Bottom hairline across entire title bar
    m_bottomLine = new QFrame(this);
    m_bottomLine->setFrameShape(QFrame::NoFrame);
    m_bottomLine->setFixedHeight(1);
    m_bottomLine->raise();

    // Apply initial theme and re-apply whenever the active theme changes
    connect(&ui::themes::ThemeManager::instance(),
            &ui::themes::ThemeManager::themeChanged,
            this, &TitleBar::applyTheme);
    applyTheme();
}

void TitleBar::setTitle(const QString &title) {
    if (m_titleLabel)
        m_titleLabel->setText(title);
}

void TitleBar::setMinMaxVisible(bool visible) {
    m_minButton->setVisible(visible);
    m_maxButton->setVisible(visible);
}

bool TitleBar::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_menuBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                // Only start drag if no menu action under cursor
                QAction *act = m_menuBar->actionAt(me->position().toPoint());
                if (act != nullptr) {
                    return QWidget::eventFilter(obj, event);
                }
                m_menuBarDragging = true;
                emit mousePressed(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            // Only intercept moves when we initiated a drag from empty menu bar area
            if (m_menuBarDragging) {
                QMouseEvent *me = static_cast<QMouseEvent *>(event);
                emit mouseMoved(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_menuBarDragging) {
                m_menuBarDragging = false;
                emit mouseReleased();
                return true;
            }
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

void TitleBar::applyTheme() {
    const auto &mgr = ui::themes::ThemeManager::instance();

    // Title bar background and text color
    const QString bg = mgr.color("mainwindow.titlebar.background");
    if (!bg.isEmpty()) {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(bg));
        // Ensure text color is inherited from the application palette
        const QString textColor = mgr.color("mainwindow.text");
        if (!textColor.isEmpty()) {
            QColor tc(textColor);
            pal.setColor(QPalette::WindowText, tc);
            pal.setColor(QPalette::ButtonText, tc);
            pal.setColor(QPalette::Text, tc);
        }
        setPalette(pal);
        setAutoFillBackground(true);
    } else {
        setAutoFillBackground(false);
    }

    // Bottom separator line
    if (m_bottomLine) {
        const QString hline = mgr.color("mainwindow.titlebar.hline");
        if (!hline.isEmpty()) {
            m_bottomLine->setStyleSheet(QString("background-color: %1;").arg(hline));
        } else {
            m_bottomLine->setStyleSheet("background-color: palette(mid);");
        }
    }

    update();
}

} // namespace ui::widgets
