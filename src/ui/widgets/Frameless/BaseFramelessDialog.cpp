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

#include "BaseFramelessDialog.h"
#include <QGuiApplication>
#include <QScreen>

namespace ui::widgets {

BaseFramelessDialog::BaseFramelessDialog(QWidget *parent)
    : QDialog(parent),
      m_titleBar(new TitleBar(this)),
      m_contentLayout(new QVBoxLayout()) {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    // Dialogs don't need minimize/maximize
    m_titleBar->setMinMaxVisible(false);
    // Menu bar is empty for dialogs but keeps the layout consistent
    m_titleBar->menuBar()->hide();

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_titleBar, 0);

    QWidget *content = new QWidget(this);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    content->setLayout(m_contentLayout);
    root->addWidget(content, 1);

    setLayout(root);

    // Wire title bar close to dialog reject
    connect(m_titleBar, &TitleBar::closeRequested, this, &QDialog::reject);
    // Wire drag
    connect(m_titleBar, &TitleBar::mousePressed, this, &BaseFramelessDialog::onTitleMousePressed);
    connect(m_titleBar, &TitleBar::mouseMoved, this, &BaseFramelessDialog::onTitleMouseMoved);
    connect(m_titleBar, &TitleBar::mouseReleased, this, &BaseFramelessDialog::onTitleMouseReleased);
}

void BaseFramelessDialog::setWindowTitle(const QString &title) {
    QDialog::setWindowTitle(title);
    m_titleBar->setTitle(title);
}

void BaseFramelessDialog::onTitleMousePressed(const QPoint &globalPos) {
    m_dragging = true;
    m_dragOffset = globalPos - frameGeometry().topLeft();
}

void BaseFramelessDialog::onTitleMouseMoved(const QPoint &globalPos) {
    if (m_dragging) {
        QPoint newPos = globalPos - m_dragOffset;
        // Clamp so the title bar stays at least partially on screen
        if (QScreen *screen = QGuiApplication::screenAt(globalPos)) {
            QRect avail = screen->availableGeometry();
            // Keep at least 40px of the title bar visible horizontally
            newPos.setX(qBound(avail.left() - width() + 40, newPos.x(), avail.right() - 40));
            // Keep the top of the window within the screen
            newPos.setY(qMax(avail.top(), newPos.y()));
        }
        move(newPos);
    }
}

void BaseFramelessDialog::onTitleMouseReleased() {
    m_dragging = false;
}

} // namespace ui::widgets
