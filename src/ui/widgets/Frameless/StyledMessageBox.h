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
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace ui::widgets {

/// Drop-in replacement for QMessageBox that uses the frameless dialog style.
class StyledMessageBox : public BaseFramelessDialog {
    Q_OBJECT
  public:
    enum Result { Yes, No, Ok };

    static void information(QWidget *parent, const QString &title, const QString &text);
    static void warning(QWidget *parent, const QString &title, const QString &text);
    static Result question(QWidget *parent, const QString &title, const QString &text);

  private:
    explicit StyledMessageBox(QWidget *parent, const QString &title,
                              const QString &text, bool hasNo = false);
    Result m_result = Ok;
};

} // namespace ui::widgets
