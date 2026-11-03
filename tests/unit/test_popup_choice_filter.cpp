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

#include "ui/widgets/popup_choice_filter.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QSignalSpy>
#include <QWheelEvent>
#include <QtTest/QtTest>

// The empty-state "Open saved session" box opens a session on activated().
// Scrolling or arrowing over the closed box must not pick entries.
class TestPopupChoiceFilter : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        m_combo = new QComboBox();
        m_combo->addItems({"(Open saved session)", "A", "B", "C"});
        new ui::widgets::PopupChoiceFilter(m_combo);
        m_combo->show();
        m_spy = new QSignalSpy(m_combo, &QComboBox::activated);
    }
    void cleanup() {
        delete m_spy;
        delete m_combo;
    }

    void wheelDoesNotPickEntries() {
        for (int i = 0; i < 3; ++i) {
            QWheelEvent ev(QPointF(5, 5), m_combo->mapToGlobal(QPointF(5, 5)), QPoint(),
                           QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(m_combo, &ev);
        }
        QCOMPARE(m_spy->count(), 0);
        QCOMPARE(m_combo->currentIndex(), 0);
    }

    void arrowKeysAndTypeAheadDoNotPickEntries() {
        QTest::keyClick(m_combo, Qt::Key_Down);
        QTest::keyClick(m_combo, Qt::Key_End);
        QTest::keyClick(m_combo, Qt::Key_B);
        m_combo->setCurrentIndex(2);
        QTest::keyClick(m_combo, Qt::Key_Up, Qt::AltModifier);
        QCOMPARE(m_combo->currentIndex(), 2);
        m_combo->setCurrentIndex(0);
        QCOMPARE(m_spy->count(), 0);
        QCOMPARE(m_combo->currentIndex(), 0);
    }

    // Choosing from the popup still works, including the same entry again.
    void popupChoiceActivates() {
        m_combo->showPopup();
        QAbstractItemView *view = m_combo->view();
        QTRY_VERIFY(view->isVisible());
        view->setCurrentIndex(view->model()->index(2, 0));
        QTest::keyClick(view, Qt::Key_Return);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(m_spy->first().at(0).toInt(), 2);
    }

  private:
    QComboBox *m_combo = nullptr;
    QSignalSpy *m_spy = nullptr;
};

QTEST_MAIN(TestPopupChoiceFilter)
#include "test_popup_choice_filter.moc"
