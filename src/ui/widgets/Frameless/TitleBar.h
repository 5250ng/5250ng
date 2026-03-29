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

#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QWidget>

namespace ui::widgets {

class TitleBar : public QWidget {
    Q_OBJECT
  public:
    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar() override = default;

    QMenuBar *menuBar() const { return m_menuBar; }
    void setTitle(const QString &title);
    void setMinMaxVisible(bool visible);

  signals:
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();
    void mousePressed(const QPoint &globalPos);
    void mouseDoubleClicked(const QPoint &globalPos);

  private slots:
    void applyTheme();

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    QMenuBar *m_menuBar;
    QLabel *m_titleLabel;
    class QFrame *m_bottomLine;
    QPushButton *m_minButton;
    QPushButton *m_maxButton;
    QPushButton *m_closeButton;
    QHBoxLayout *m_layout;
};

} // namespace ui::widgets
