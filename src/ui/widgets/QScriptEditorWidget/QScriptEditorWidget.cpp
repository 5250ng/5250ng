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

#include "QScriptEditorWidget.h"
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ui::widgets {

using core::scripting::Diagnostic;

namespace {
constexpr int kLineRole = Qt::UserRole;
constexpr int kColumnRole = Qt::UserRole + 1;
} // namespace

QScriptEditorWidget::QScriptEditorWidget(QWidget *parent) : QWidget(parent) {
    m_editor = new ScriptCodeEditor(this);

    QWidget *problemsPane = new QWidget(this);
    QVBoxLayout *problemsLayout = new QVBoxLayout(problemsPane);
    problemsLayout->setContentsMargins(0, 4, 0, 0);
    problemsLayout->setSpacing(2);
    m_problemsHeader = new QLabel(tr("Problems"), problemsPane);
    m_problems = new QTreeWidget(problemsPane);
    m_problems->setRootIsDecorated(false);
    m_problems->setHeaderLabels({tr("Line"), tr("Message")});
    m_problems->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_problems->header()->setStretchLastSection(true);
    m_problems->setUniformRowHeights(true);
    // Room for the header and about four problems before scrolling
    m_problems->setMinimumHeight(m_problems->fontMetrics().height() * 7);
    problemsLayout->addWidget(m_problemsHeader);
    problemsLayout->addWidget(m_problems);

    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_editor);
    splitter->addWidget(problemsPane);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);

    connect(m_editor, &ScriptCodeEditor::diagnosticsChanged, this, &QScriptEditorWidget::showDiagnostics);
    connect(m_problems, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        m_editor->goToLine(item->data(0, kLineRole).toInt(), item->data(0, kColumnRole).toInt());
    });
    showDiagnostics({});
}

void QScriptEditorWidget::setText(const QString &text) {
    m_editor->setPlainText(text);
    m_editor->lintNow();
}

void QScriptEditorWidget::showDiagnostics(const QVector<Diagnostic> &diagnostics) {
    const QIcon errorIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical);
    const QIcon warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    m_problems->clear();
    m_errorCount = 0;
    m_warningCount = 0;
    for (const Diagnostic &d : diagnostics) {
        const bool error = d.severity == Diagnostic::Severity::Error;
        error ? ++m_errorCount : ++m_warningCount;
        auto *item = new QTreeWidgetItem(m_problems, {QString::number(d.line), d.message});
        item->setIcon(0, error ? errorIcon : warningIcon);
        item->setData(0, kLineRole, d.line);
        item->setData(0, kColumnRole, d.column);
        item->setToolTip(1, d.message);
    }
    if (diagnostics.isEmpty())
        m_problemsHeader->setText(tr("Problems: none"));
    else
        m_problemsHeader->setText(tr("Problems: %1 error(s), %2 warning(s)")
                                      .arg(m_errorCount).arg(m_warningCount));
    emit problemsChanged(m_errorCount, m_warningCount);
}

} // namespace ui::widgets
