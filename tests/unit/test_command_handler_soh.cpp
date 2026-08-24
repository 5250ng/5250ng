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

#include "ui/rendering/tn5250_command_handler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"

#include <QtTest/QtTest>

using ui::rendering::TN5250CommandHandler;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #133: the SOH error-row byte is host-controlled
// and was stored unclamped, driving ScreenBuffer::cell() (no release-mode
// bounds check) out of bounds in Write Error Code and error reset handling.
class TestCommandHandlerSoh : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testSohErrorRowClampedToScreen();
    void testWriteErrorCodeWithHostileErrorRow();
    void testSohValidErrorRowKept();
    void testGddmBlockBypassesAlphanumericRendererAndPaces();
    void testGddmSuppressPacingOrder();
    void testGddmReadStatusWithAlphanumericPrefix();

  private:
    Q5250ScreenWidget *m_widget = nullptr;
    TN5250CommandHandler *m_handler = nullptr;
};

void TestCommandHandlerSoh::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_handler = new TN5250CommandHandler(this);
    m_handler->setDisplayWidget(m_widget);
}

void TestCommandHandlerSoh::cleanup() {
    delete m_handler;
    m_handler = nullptr;
    delete m_widget;
    m_widget = nullptr;
}

void TestCommandHandlerSoh::testSohErrorRowClampedToScreen() {
    // Host byte far beyond the 24-row screen
    m_handler->onSohReceived(0xFE, 0, 0, 0);
    QVERIFY(m_widget->errorLineRow() >= 0);
    QVERIFY(m_widget->errorLineRow() < m_widget->screenBuffer()->rows());
    QCOMPARE(m_widget->errorLineRow(), m_widget->screenBuffer()->rows() - 1);
}

void TestCommandHandlerSoh::testWriteErrorCodeWithHostileErrorRow() {
    // SOH with hostile row followed by Write Error Code: before the fix this
    // read screen cells at row 253 of a 24-row buffer (and the later error
    // reset wrote there). Must stay inside the buffer and not crash.
    m_handler->onSohReceived(0xFE, 0, 0, 0);
    QByteArray errorCode;
    errorCode.append(static_cast<char>(0xC5)); // EBCDIC 'E'
    errorCode.append(static_cast<char>(0xD9)); // EBCDIC 'R'
    m_handler->onWriteErrorCode(errorCode);

    int lastRow = m_widget->screenBuffer()->rows() - 1;
    QCOMPARE(m_widget->screenBuffer()->character(lastRow, 0),
             static_cast<uint8_t>(0xC5));
    QCOMPARE(m_widget->screenBuffer()->character(lastRow, 1),
             static_cast<uint8_t>(0xD9));
}

void TestCommandHandlerSoh::testSohValidErrorRowKept() {
    // A legal SOH error row (1-based on the wire) must still land unchanged
    m_handler->onSohReceived(24, 0, 0, 0);
    QCOMPARE(m_widget->errorLineRow(), 23);
    m_handler->onSohReceived(1, 0, 0, 0);
    QCOMPARE(m_widget->errorLineRow(), 0);
}

void TestCommandHandlerSoh::testGddmBlockBypassesAlphanumericRendererAndPaces() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });

    m_handler->handleRawScreenData(QByteArray::fromHex(
        "ff93b041a04040404a41644072425640599295"));

    QVERIFY(m_widget->gddmGraphicsVisible());
    QCOMPARE(m_widget->gddmGraphicsPlane().pixelColor(0, 277), QColor(255, 0, 0));
    QCOMPARE(m_widget->screenBuffer()->character(0, 0), static_cast<uint8_t>(0x40));
    QCOMPARE(responses.size(), 1);
    QCOMPARE(responses[0].size(), 3);
    QCOMPARE(static_cast<uint8_t>(responses[0][2]), static_cast<uint8_t>(0x3C));

    m_widget->resize(800, 480);
    QImage composed(m_widget->size(), QImage::Format_ARGB32_Premultiplied);
    composed.fill(Qt::black);
    m_widget->render(&composed);
    bool foundRedGraphicsPixel = false;
    for (int y = 0; y < composed.height() && !foundRedGraphicsPixel; ++y) {
        for (int x = 0; x < composed.width(); ++x) {
            const QColor pixel = composed.pixelColor(x, y);
            if (pixel.red() > 200 && pixel.green() < 40 && pixel.blue() < 40) {
                foundRedGraphicsPixel = true;
                break;
            }
        }
    }
    QVERIFY(foundRedGraphicsPixel);
}

void TestCommandHandlerSoh::testGddmSuppressPacingOrder() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });

    m_handler->handleRawScreenData(QByteArray::fromHex("ff9695"));

    QVERIFY(responses.isEmpty());
    QCOMPARE(m_widget->screenBuffer()->character(0, 0), static_cast<uint8_t>(0x40));
}

void TestCommandHandlerSoh::testGddmReadStatusWithAlphanumericPrefix() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });

    m_handler->handleRawScreenData(QByteArray::fromHex(
        "1101031d4800270032ff804043959090909090"));

    QCOMPARE(responses.size(), 1);
    QCOMPARE(static_cast<uint8_t>(responses[0][2]), static_cast<uint8_t>(0x3C));
    QVERIFY(responses[0].size() > 10);
    QCOMPARE(static_cast<uint8_t>(responses[0][3]), static_cast<uint8_t>(0x11));
    QCOMPARE(static_cast<uint8_t>(responses[0][6]), static_cast<uint8_t>(0xFF));
    QCOMPARE(static_cast<uint8_t>(responses[0][8]), static_cast<uint8_t>(0xFF));
    QCOMPARE(static_cast<uint8_t>(responses[0][9]), static_cast<uint8_t>(0xF2));
    QCOMPARE(static_cast<uint8_t>(responses[0][10]), static_cast<uint8_t>(0x80));
    QCOMPARE(m_widget->screenBuffer()->character(0, 3), static_cast<uint8_t>(0xFF));
    QCOMPARE(m_widget->screenBuffer()->character(0, 6), static_cast<uint8_t>(0xF2));
}

QTEST_MAIN(TestCommandHandlerSoh)
#include "test_command_handler_soh.moc"
