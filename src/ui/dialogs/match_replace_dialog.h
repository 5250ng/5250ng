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

#include "core/match_replace_engine.h"
#include <QtUiStyle/BaseFramelessDialog.h>
#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ui::dialogs {

class MatchReplaceDialog : public qt_ui_style::BaseFramelessDialog {
    Q_OBJECT

  public:
    explicit MatchReplaceDialog(const QVector<core::MatchReplaceRule> &rules,
                                QWidget *parent = nullptr);

    QVector<core::MatchReplaceRule> rules() const;

  private slots:
    void onAddRule();
    void onRemoveRule();
    void onMoveUp();
    void onMoveDown();

  private:
    void populateTable(const QVector<core::MatchReplaceRule> &rules);
    void addRuleRow(const core::MatchReplaceRule &rule);

    QTableWidget *m_table;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_upBtn;
    QPushButton *m_downBtn;
};

} // namespace ui::dialogs
