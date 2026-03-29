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

#include "ui/widgets/Frameless/TitleBar.h"
#include <QDialog>
#include <QVBoxLayout>

namespace ui::widgets {

class BaseFramelessDialog : public QDialog {
    Q_OBJECT
  public:
    explicit BaseFramelessDialog(QWidget *parent = nullptr);
    ~BaseFramelessDialog() override = default;

    void setWindowTitle(const QString &title);
    TitleBar *titleBar() const { return m_titleBar; }
    QVBoxLayout *contentLayout() const { return m_contentLayout; }

  private:
    TitleBar *m_titleBar;
    QVBoxLayout *m_contentLayout;

  private slots:
    void onTitleMousePressed(const QPoint &globalPos);
};

} // namespace ui::widgets
