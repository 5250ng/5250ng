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

#include "QVirtualKeyboardWidget.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

namespace ui::widgets {

using core::KeyboardMapping;
using core::KeyChord;
using core::MappedAction;

QVirtualKeyboardWidget::QVirtualKeyboardWidget(QWidget *parent) : QWidget(parent) {
    buildLayout();
    refreshChordLabels();
}

QPushButton *QVirtualKeyboardWidget::addActionKey(QGridLayout *grid, int row, int col,
                                                  int rowSpan, int colSpan,
                                                  const QString &label, MappedAction action) {
    QPushButton *btn = new QPushButton(label, this);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setMinimumSize(36, 28);
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QPushButton::clicked, this, [this, action]() {
        emit actionTriggered(action);
    });
    connect(btn, &QPushButton::customContextMenuRequested, this, [this, btn, action](const QPoint &) {
        QMenu menu(btn);
        QAction *remap = menu.addAction(tr("Remap…"));
        if (menu.exec(QCursor::pos()) == remap) {
            emit remapRequested(action);
        }
    });
    grid->addWidget(btn, row, col, rowSpan, colSpan);
    m_actionButtons.append({btn, action, label});
    return btn;
}

QPushButton *QVirtualKeyboardWidget::addCharKey(QGridLayout *grid, int row, int col,
                                                int rowSpan, int colSpan,
                                                QChar unshifted, QChar shifted) {
    QPushButton *btn = new QPushButton(QString(unshifted), this);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setMinimumSize(32, 28);
    connect(btn, &QPushButton::clicked, this, [this, btn, unshifted, shifted]() {
        QChar ch = applyModifiers(unshifted, shifted);
        emit characterTriggered(ch);
        if (m_shiftHeld) {
            m_shiftHeld = false;
            if (m_shiftBtn) m_shiftBtn->setChecked(false);
        }
        btn->clearFocus();
    });
    grid->addWidget(btn, row, col, rowSpan, colSpan);
    return btn;
}

QPushButton *QVirtualKeyboardWidget::addModifierKey(QGridLayout *grid, int row, int col,
                                                    int rowSpan, int colSpan,
                                                    const QString &label,
                                                    Qt::KeyboardModifier mod) {
    QPushButton *btn = new QPushButton(label, this);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCheckable(true);
    btn->setMinimumSize(48, 28);
    connect(btn, &QPushButton::toggled, this, [this, mod](bool on) {
        if (mod == Qt::ShiftModifier) m_shiftHeld = on;
        else if (mod == Qt::ControlModifier) m_ctrlHeld = on;
        else if (mod == Qt::AltModifier) m_altHeld = on;
    });
    grid->addWidget(btn, row, col, rowSpan, colSpan);
    return btn;
}

QChar QVirtualKeyboardWidget::applyModifiers(QChar unshifted, QChar shifted) const {
    if (m_shiftHeld && shifted != QChar(0)) return shifted;
    if (m_shiftHeld && unshifted.isLetter()) return unshifted.toUpper();
    return unshifted;
}

void QVirtualKeyboardWidget::buildLayout() {
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // --- Row of PF / command keys (Model M "F-row" extended) ----------------
    QLabel *title = new QLabel(tr("Virtual 5250 Keyboard"), this);
    title->setStyleSheet("font-weight: bold;");
    root->addWidget(title);

    QGridLayout *fnGrid = new QGridLayout();
    fnGrid->setHorizontalSpacing(3);
    fnGrid->setVerticalSpacing(3);
    // PF1..PF24 on two short rows
    for (int i = 0; i < 12; ++i) {
        addActionKey(fnGrid, 0, i, 1, 1, QString("PF%1").arg(i + 1),
                     static_cast<MappedAction>(static_cast<int>(MappedAction::PF1) + i));
        addActionKey(fnGrid, 1, i, 1, 1, QString("PF%1").arg(i + 13),
                     static_cast<MappedAction>(static_cast<int>(MappedAction::PF13) + i));
    }
    root->addLayout(fnGrid);

    // --- Main typing area ---------------------------------------------------
    QGridLayout *main = new QGridLayout();
    main->setHorizontalSpacing(3);
    main->setVerticalSpacing(3);

    // Digit row
    struct CK { const char *u; const char *s; };
    const CK digitRow[] = {
        {"`","~"},{"1","!"},{"2","@"},{"3","#"},{"4","$"},{"5","%"},
        {"6","^"},{"7","&"},{"8","*"},{"9","("},{"0",")"},{"-","_"},{"=","+"},
    };
    for (int i = 0; i < 13; ++i) {
        addCharKey(main, 0, i, 1, 1, QChar(digitRow[i].u[0]), QChar(digitRow[i].s[0]));
    }
    addActionKey(main, 0, 13, 1, 2, tr("Backspace"), MappedAction::Backspace);

    // Q row
    addActionKey(main, 1, 0, 1, 1, tr("Tab"), MappedAction::Tab);
    const char qRow[] = "qwertyuiop";
    for (int i = 0; i < 10; ++i) {
        addCharKey(main, 1, 1 + i, 1, 1, QChar(qRow[i]), QChar(qRow[i]).toUpper());
    }
    addCharKey(main, 1, 11, 1, 1, '[', '{');
    addCharKey(main, 1, 12, 1, 1, ']', '}');
    addCharKey(main, 1, 13, 1, 2, '\\', '|');

    // A row
    addActionKey(main, 2, 0, 1, 1, tr("Reset"), MappedAction::Reset);
    const char aRow[] = "asdfghjkl";
    for (int i = 0; i < 9; ++i) {
        addCharKey(main, 2, 1 + i, 1, 1, QChar(aRow[i]), QChar(aRow[i]).toUpper());
    }
    addCharKey(main, 2, 10, 1, 1, ';', ':');
    addCharKey(main, 2, 11, 1, 1, '\'', '"');
    addActionKey(main, 2, 12, 1, 3, tr("Enter"), MappedAction::Enter);

    // Z row
    m_shiftBtn = addModifierKey(main, 3, 0, 1, 1, tr("Shift"), Qt::ShiftModifier);
    const char zRow[] = "zxcvbnm";
    for (int i = 0; i < 7; ++i) {
        addCharKey(main, 3, 1 + i, 1, 1, QChar(zRow[i]), QChar(zRow[i]).toUpper());
    }
    addCharKey(main, 3, 8, 1, 1, ',', '<');
    addCharKey(main, 3, 9, 1, 1, '.', '>');
    addCharKey(main, 3, 10, 1, 1, '/', '?');
    addActionKey(main, 3, 11, 1, 1, tr("Field+"), MappedAction::FieldPlus);
    addActionKey(main, 3, 12, 1, 1, tr("Field-"), MappedAction::FieldMinus);
    addActionKey(main, 3, 13, 1, 2, tr("Field Exit"), MappedAction::FieldExit);

    // Control row
    m_ctrlBtn = addModifierKey(main, 4, 0, 1, 1, tr("Ctrl"), Qt::ControlModifier);
    m_altBtn = addModifierKey(main, 4, 1, 1, 1, tr("Alt"), Qt::AltModifier);
    addCharKey(main, 4, 2, 1, 9, QChar(' '), QChar(0));
    addActionKey(main, 4, 11, 1, 1, tr("Ins"), MappedAction::Insert);
    addActionKey(main, 4, 12, 1, 1, tr("Del"), MappedAction::Delete);
    addActionKey(main, 4, 13, 1, 1, tr("Dup"), MappedAction::Dup);
    addActionKey(main, 4, 14, 1, 1, tr("BckTab"), MappedAction::BackTab);

    root->addLayout(main, 1);

    // --- Navigation / AS-400 control cluster on the right ------------------
    QGridLayout *nav = new QGridLayout();
    nav->setHorizontalSpacing(3);
    nav->setVerticalSpacing(3);
    addActionKey(nav, 0, 0, 1, 1, tr("Attn"), MappedAction::Attn);
    addActionKey(nav, 0, 1, 1, 1, tr("SysReq"), MappedAction::SysReq);
    addActionKey(nav, 0, 2, 1, 1, tr("Clear"), MappedAction::Clear);
    addActionKey(nav, 0, 3, 1, 1, tr("Help"), MappedAction::Help);
    addActionKey(nav, 0, 4, 1, 1, tr("Print"), MappedAction::Print);
    addActionKey(nav, 1, 0, 1, 1, tr("Home"), MappedAction::Home);
    addActionKey(nav, 1, 1, 1, 1, tr("End"), MappedAction::End);
    addActionKey(nav, 1, 2, 1, 1, tr("Roll↑"), MappedAction::RollUp);
    addActionKey(nav, 1, 3, 1, 1, tr("Roll↓"), MappedAction::RollDown);
    addActionKey(nav, 1, 4, 1, 1, tr("ErFld"), MappedAction::EraseField);
    addActionKey(nav, 2, 0, 1, 1, tr("ErEOF"), MappedAction::EraseEOF);
    addActionKey(nav, 2, 1, 1, 1, tr("ErInp"), MappedAction::EraseInput);
    addActionKey(nav, 2, 2, 1, 1, tr("↑"), MappedAction::ArrowUp);
    addActionKey(nav, 2, 3, 1, 1, tr("↓"), MappedAction::ArrowDown);
    addActionKey(nav, 2, 4, 1, 1, tr("←"), MappedAction::ArrowLeft);
    addActionKey(nav, 2, 5, 1, 1, tr("→"), MappedAction::ArrowRight);
    root->addLayout(nav);
}

void QVirtualKeyboardWidget::refreshChordLabels() {
    const auto &mapping = KeyboardMapping::instance();
    for (const auto &entry : m_actionButtons) {
        const QList<KeyChord> chords = mapping.chordsFor(entry.action);
        QString tip = QObject::tr("Click to send %1").arg(entry.baseLabel);
        if (chords.isEmpty()) {
            tip += QObject::tr("\nHost chord: (unbound)");
        } else {
            QStringList names;
            names.reserve(chords.size());
            for (const auto &c : chords) names.append(c.toString());
            tip += QObject::tr("\nHost chord: %1").arg(names.join(QStringLiteral(", ")));
        }
        entry.button->setToolTip(tip);
    }
}

} // namespace ui::widgets
