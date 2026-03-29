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
#include <QPainter>
#include <QPixmap>
#include "ui/themes/manager.h"

namespace ui::widgets {

// Theme-aware button style: transparent background, subtle hover, themed close hover
static QString windowButtonStyle(const QString &hoverBg) {
    return QString(
        "QPushButton {"
        "  background-color: transparent;"
        "  border: none;"
        "  border-radius: 3px;"
        "  padding: 0px 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %1;"
        "}"
    ).arg(hoverBg);
}

// Generate window button icons with a specific color
static QIcon makeMinimizeIcon(const QColor &color, int size) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.2, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    int m = size / 5; // margin
    p.drawLine(m, size / 2, size - m, size / 2);
    return QIcon(pix);
}

static QIcon makeMaximizeIcon(const QColor &color, int size) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    int m = size / 5;
    p.drawRoundedRect(m, m, size - 2 * m, size - 2 * m, 0.5, 0.5);
    return QIcon(pix);
}

static QIcon makeCloseIcon(const QColor &color, int size) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.2, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    int m = size / 4;
    p.drawLine(m, m, size - m, size - m);
    p.drawLine(size - m, m, m, size - m);
    return QIcon(pix);
}

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent),
      m_menuBar(new QMenuBar(this)),
      m_minButton(new QPushButton(this)),
      m_maxButton(new QPushButton(this)),
      m_closeButton(new QPushButton(this)),
      m_layout(new QHBoxLayout(this)) {
    m_layout->setContentsMargins(8, 2, 8, 2);
    m_layout->setSpacing(4);
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

    // Window control buttons (right side)
    const int btnW = 28;
    const int btnH = 18;
    const int iconSz = 10;
    for (auto *btn : {m_minButton, m_maxButton, m_closeButton}) {
        btn->setFixedSize(btnW, btnH);
        btn->setFlat(true);
        btn->setIconSize(QSize(iconSz, iconSz));
        btn->setCursor(Qt::ArrowCursor);
    }

    // Icons are set in applyTheme() to match the current text color

    m_layout->addWidget(m_minButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_maxButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    setLayout(m_layout);

    connect(m_minButton, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maxButton, &QPushButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(m_closeButton, &QPushButton::clicked, this, &TitleBar::closeRequested);

    // Install event filter on the menubar to detect press/dblclick on empty areas
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
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                // Only start drag if no menu action under cursor
                if (m_menuBar->actionAt(me->position().toPoint()) != nullptr)
                    return QWidget::eventFilter(obj, event);
                emit mousePressed(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_menuBar->actionAt(me->position().toPoint()) != nullptr)
                return QWidget::eventFilter(obj, event);
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

    // Button styling: theme-aware hover, reddish close hover
    QString btnHover = mgr.color("mainwindow.titlebar.button.hover");
    if (btnHover.isEmpty()) btnHover = QStringLiteral("rgba(255,255,255,40)");
    QString closeHover = mgr.color("mainwindow.titlebar.close.hover");
    if (closeHover.isEmpty()) closeHover = QStringLiteral("rgba(232,17,35,200)");

    m_minButton->setStyleSheet(windowButtonStyle(btnHover));
    m_maxButton->setStyleSheet(windowButtonStyle(btnHover));
    m_closeButton->setStyleSheet(windowButtonStyle(closeHover));

    // Regenerate button icons using the current text color
    QColor iconColor = palette().color(QPalette::ButtonText);
    int iconSz = m_minButton->iconSize().width();
    m_minButton->setIcon(makeMinimizeIcon(iconColor, iconSz));
    m_maxButton->setIcon(makeMaximizeIcon(iconColor, iconSz));
    m_closeButton->setIcon(makeCloseIcon(iconColor, iconSz));

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
