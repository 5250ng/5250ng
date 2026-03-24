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

#include "BaseFramelessDialog.h"
#include <QLineEdit>
#include <QString>

namespace ui::widgets {

class StyledInputDialog : public BaseFramelessDialog {
    Q_OBJECT
  public:
    static QString getText(QWidget *parent, const QString &title,
                           const QString &label,
                           QLineEdit::EchoMode mode = QLineEdit::Normal,
                           const QString &text = {},
                           bool *ok = nullptr);

  private:
    explicit StyledInputDialog(QWidget *parent, const QString &title,
                               const QString &label, QLineEdit::EchoMode mode,
                               const QString &text);
    QLineEdit *m_lineEdit;
};

} // namespace ui::widgets
