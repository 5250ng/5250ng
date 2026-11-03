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

#include "ui/rendering/tn5250_stream_renderer.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"

#include <QtTest/QtTest>

using ui::rendering::TN5250StreamRenderer;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #360: a non-display input field defined by a
// Start Field order with no field data after it kept nonDisplay = false on
// its cells, so characters typed into it were painted in clear.
class TestStreamRendererNonDisplay : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testEmptyNonDisplayInputFieldHidesItsCells();
    void testFilledNonDisplayInputFieldStillHidden();
    void testVisibleFieldRedefinitionShowsCells();
    void testProtectedFieldCellsUnchanged();
    void testProtectedFieldOverEmptyNonDisplayFieldHasNoMarkers();

  private:
    // SBA to (row, col), 1-based, then SF with the given FFW, attribute and
    // length, and nothing after it.
    static QByteArray sbaSf(int row, int col, uint8_t ffw1, uint8_t attr, int len);

    Q5250ScreenWidget *m_widget = nullptr;
};

QByteArray TestStreamRendererNonDisplay::sbaSf(int row, int col, uint8_t ffw1, uint8_t attr, int len) {
    QByteArray d;
    d.append(char(0x11)).append(char(row)).append(char(col));
    d.append(char(0x1D)).append(char(ffw1)).append(char(0x00)).append(char(attr));
    d.append(char((len >> 8) & 0xFF)).append(char(len & 0xFF));
    return d;
}

void TestStreamRendererNonDisplay::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
}

void TestStreamRendererNonDisplay::cleanup() {
    delete m_widget;
    m_widget = nullptr;
}

void TestStreamRendererNonDisplay::testEmptyNonDisplayInputFieldHidesItsCells() {
    // SF at row 5 col 10 (0-based (4,9)): the attribute byte sits there and
    // the 10-cell field starts at (4,10).
    TN5250StreamRenderer(m_widget).render(sbaSf(5, 10, 0x40, 0x27, 10));
    auto *screen = m_widget->screenBuffer();
    for (int c = 10; c < 20; ++c)
        QVERIFY2(screen->cell(4, c).attributes.nonDisplay, qPrintable(QString("col %1").arg(c)));

    // A character typed into the field keeps the cell's attributes.
    screen->writeChar(4, 10, 0xC1);
    QVERIFY(screen->cell(4, 10).attributes.nonDisplay);
}

void TestStreamRendererNonDisplay::testFilledNonDisplayInputFieldStillHidden() {
    QByteArray d = sbaSf(5, 10, 0x40, 0x27, 10);
    d.append(QByteArray(10, char(0x00)));
    TN5250StreamRenderer(m_widget).render(d);
    auto *screen = m_widget->screenBuffer();
    for (int c = 10; c < 20; ++c)
        QVERIFY(screen->cell(4, c).attributes.nonDisplay);
}

void TestStreamRendererNonDisplay::testVisibleFieldRedefinitionShowsCells() {
    // A later write redefines the same position as a visible input field
    // (attribute 0x24, underline): its cells must no longer be hidden.
    TN5250StreamRenderer(m_widget).render(sbaSf(5, 10, 0x40, 0x27, 10));
    TN5250StreamRenderer(m_widget).render(sbaSf(5, 10, 0x40, 0x24, 10));
    auto *screen = m_widget->screenBuffer();
    for (int c = 10; c < 20; ++c)
        QVERIFY(!screen->cell(4, c).attributes.nonDisplay);
}

void TestStreamRendererNonDisplay::testProtectedFieldCellsUnchanged() {
    // Protected (FFW bypass) non-display field without data: its cells are
    // not flagged, so the inheritance pass does not mistake them for markers.
    TN5250StreamRenderer(m_widget).render(sbaSf(5, 10, 0x60, 0x27, 10));
    auto *screen = m_widget->screenBuffer();
    for (int c = 10; c < 20; ++c)
        QVERIFY(!screen->cell(4, c).attributes.nonDisplay);
}

void TestStreamRendererNonDisplay::testProtectedFieldOverEmptyNonDisplayFieldHasNoMarkers() {
    // An empty password field later covered, without a Clear Unit, by a
    // protected underlined field: the field cells must stay data cells, not
    // turn into attribute markers that break attribute inheritance.
    TN5250StreamRenderer(m_widget).render(sbaSf(5, 10, 0x40, 0x27, 10));
    TN5250StreamRenderer(m_widget).render(sbaSf(5, 10, 0x60, 0x24, 10));
    auto *screen = m_widget->screenBuffer();
    for (int c = 10; c < 20; ++c) {
        const auto &a = screen->cell(4, c).attributes;
        QVERIFY2(!(a.nonDisplay && a.protected_field), qPrintable(QString("col %1 is a marker").arg(c)));
        QVERIFY2(a.underline, qPrintable(QString("col %1 lost its underline").arg(c)));
    }
}

QTEST_MAIN(TestStreamRendererNonDisplay)
#include "test_stream_renderer_nondisplay.moc"
