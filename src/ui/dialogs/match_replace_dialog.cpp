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

#include "match_replace_dialog.h"

namespace ui::dialogs {

MatchReplaceDialog::MatchReplaceDialog(const QVector<core::MatchReplaceRule> &rules,
                                       QWidget *parent)
    : BaseFramelessDialog(parent) {
    setWindowTitle("Match and Replace - Patterns");
    resize(700, 400);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(8, 8, 8, 8);
    content->setSpacing(6);

    // Table
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"Enabled", "Pattern", "Replacement", "Regex", "Case Sensitive"});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    content->addWidget(m_table, 1);

    // Buttons row: Add/Remove/Up/Down on left, OK/Cancel on right
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(4);

    m_addBtn = new QPushButton("Add", this);
    m_removeBtn = new QPushButton("Remove", this);
    m_upBtn = new QPushButton("Up", this);
    m_downBtn = new QPushButton("Down", this);

    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addWidget(m_upBtn);
    btnLayout->addWidget(m_downBtn);
    btnLayout->addStretch();

    auto *okBtn = new QPushButton("OK", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);

    content->addLayout(btnLayout);

    connect(m_addBtn, &QPushButton::clicked, this, &MatchReplaceDialog::onAddRule);
    connect(m_removeBtn, &QPushButton::clicked, this, &MatchReplaceDialog::onRemoveRule);
    connect(m_upBtn, &QPushButton::clicked, this, &MatchReplaceDialog::onMoveUp);
    connect(m_downBtn, &QPushButton::clicked, this, &MatchReplaceDialog::onMoveDown);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    populateTable(rules);
}

void MatchReplaceDialog::populateTable(const QVector<core::MatchReplaceRule> &rules) {
    m_table->setRowCount(0);
    for (const auto &rule : rules) {
        addRuleRow(rule);
    }
}

void MatchReplaceDialog::addRuleRow(const core::MatchReplaceRule &rule) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    // Enabled checkbox
    auto *enabledCheck = new QCheckBox(this);
    enabledCheck->setChecked(rule.enabled);
    auto *enabledWidget = new QWidget(this);
    auto *enabledLayout = new QHBoxLayout(enabledWidget);
    enabledLayout->addWidget(enabledCheck);
    enabledLayout->setAlignment(Qt::AlignCenter);
    enabledLayout->setContentsMargins(0, 0, 0, 0);
    m_table->setCellWidget(row, 0, enabledWidget);

    // Pattern
    m_table->setItem(row, 1, new QTableWidgetItem(rule.pattern));

    // Replacement
    m_table->setItem(row, 2, new QTableWidgetItem(rule.replacement));

    // Regex checkbox
    auto *regexCheck = new QCheckBox(this);
    regexCheck->setChecked(rule.isRegex);
    auto *regexWidget = new QWidget(this);
    auto *regexLayout = new QHBoxLayout(regexWidget);
    regexLayout->addWidget(regexCheck);
    regexLayout->setAlignment(Qt::AlignCenter);
    regexLayout->setContentsMargins(0, 0, 0, 0);
    m_table->setCellWidget(row, 3, regexWidget);

    // Case sensitive checkbox
    auto *caseCheck = new QCheckBox(this);
    caseCheck->setChecked(rule.caseSensitive);
    auto *caseWidget = new QWidget(this);
    auto *caseLayout = new QHBoxLayout(caseWidget);
    caseLayout->addWidget(caseCheck);
    caseLayout->setAlignment(Qt::AlignCenter);
    caseLayout->setContentsMargins(0, 0, 0, 0);
    m_table->setCellWidget(row, 4, caseWidget);
}

QVector<core::MatchReplaceRule> MatchReplaceDialog::rules() const {
    QVector<core::MatchReplaceRule> result;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        core::MatchReplaceRule rule;

        // Enabled
        auto *enabledWidget = m_table->cellWidget(row, 0);
        auto *enabledCheck = enabledWidget->findChild<QCheckBox *>();
        rule.enabled = enabledCheck ? enabledCheck->isChecked() : true;

        // Pattern
        auto *patternItem = m_table->item(row, 1);
        rule.pattern = patternItem ? patternItem->text() : QString();

        // Replacement
        auto *replItem = m_table->item(row, 2);
        rule.replacement = replItem ? replItem->text() : QString();

        // Regex
        auto *regexWidget = m_table->cellWidget(row, 3);
        auto *regexCheck = regexWidget->findChild<QCheckBox *>();
        rule.isRegex = regexCheck ? regexCheck->isChecked() : false;

        // Case sensitive
        auto *caseWidget = m_table->cellWidget(row, 4);
        auto *caseCheck = caseWidget->findChild<QCheckBox *>();
        rule.caseSensitive = caseCheck ? caseCheck->isChecked() : false;

        result.append(rule);
    }
    return result;
}

void MatchReplaceDialog::onAddRule() {
    addRuleRow(core::MatchReplaceRule());
    m_table->selectRow(m_table->rowCount() - 1);
    m_table->editItem(m_table->item(m_table->rowCount() - 1, 1));
}

void MatchReplaceDialog::onRemoveRule() {
    int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
    }
}

void MatchReplaceDialog::onMoveUp() {
    int row = m_table->currentRow();
    if (row <= 0) return;

    // Extract both rows as rules, swap, repopulate
    auto allRules = rules();
    std::swap(allRules[row], allRules[row - 1]);
    populateTable(allRules);
    m_table->selectRow(row - 1);
}

void MatchReplaceDialog::onMoveDown() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() - 1) return;

    auto allRules = rules();
    std::swap(allRules[row], allRules[row + 1]);
    populateTable(allRules);
    m_table->selectRow(row + 1);
}

} // namespace ui::dialogs
