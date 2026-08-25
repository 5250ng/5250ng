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

#include <QDir>
#include <QFile>
#include <QSignalSpy>

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
    void testGddmStatusWriteIsIndependentOfPacing();
    void testGddmResetAndErrorUseDocumentedAids();
    void testEBCDICffInTextDoesNotBeginGraphics();

    void testGddmGraphicsPlaneRendersBehindText();
    void testGddmScreenCopyEmitsRequestAndWritesNothing();
    void testCompositeExportContainsBothPlanes();

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

void TestCommandHandlerSoh::testGddmStatusWriteIsIndependentOfPacing() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });

    m_handler->handleRawScreenData(QByteArray::fromHex(
        "1101031d4800270032ff8040439695"));

    QVERIFY(responses.isEmpty());
    QCOMPARE(m_widget->screenBuffer()->character(0, 3), static_cast<uint8_t>(0xFF));
    QCOMPARE(m_widget->screenBuffer()->character(0, 6), static_cast<uint8_t>(0xF2));
}

void TestCommandHandlerSoh::testGddmResetAndErrorUseDocumentedAids() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });

    m_handler->handleRawScreenData(QByteArray::fromHex("ffff"));
    QCOMPARE(responses.size(), 1);
    QCOMPARE(static_cast<uint8_t>(responses[0][2]), static_cast<uint8_t>(0x38));

    m_handler->handleRawScreenData(QByteArray::fromHex("ffa04040404a92"));
    QCOMPARE(responses.size(), 2);
    QCOMPARE(static_cast<uint8_t>(responses[1][2]), static_cast<uint8_t>(0x3A));
}

void TestCommandHandlerSoh::testEBCDICffInTextDoesNotBeginGraphics() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });

    m_handler->handleRawScreenData(QByteArray::fromHex("c1ffc2"));

    QVERIFY(responses.isEmpty());
    QVERIFY(!m_widget->gddmGraphicsVisible());
    QCOMPARE(m_widget->screenBuffer()->character(0, 0), static_cast<uint8_t>(0xC1));
    QCOMPARE(m_widget->screenBuffer()->character(0, 1), static_cast<uint8_t>(0xFF));
    QCOMPARE(m_widget->screenBuffer()->character(0, 2), static_cast<uint8_t>(0xC2));
}

void TestCommandHandlerSoh::testGddmGraphicsPlaneRendersBehindText() {
    // Put a glyph at the centre of the screen, which is certainly inside the
    // scaled picture's target rectangle. A corner cell would sit in the
    // letterboxed margin the picture never covers and would survive either
    // way, making the test pass regardless of the layering.
    const int glyphRow = m_widget->screenBuffer()->rows() / 2;
    const int glyphCol = m_widget->screenBuffer()->cols() / 2;
    m_widget->screenBuffer()->writeChar(glyphRow, glyphCol, 0xC1); // EBCDIC 'A'

    // Write Background in colour 1, red by default, then display on. The plane
    // is opaque and covers the whole surface.
    m_handler->handleRawScreenData(QByteArray::fromHex("ff93a34195"));
    QVERIFY(m_widget->gddmGraphicsVisible());

    m_widget->resize(720, 480);
    QImage shot(m_widget->size(), QImage::Format_ARGB32);
    shot.fill(Qt::black);
    m_widget->render(&shot);

    // Scan only around the centre. Scanning the whole image would also pick up
    // the blue graphics-mode indicator in the bottom row, whose blue channel
    // would satisfy the glyph test on its own.
    const int cx = shot.width() / 2;
    const int cy = shot.height() / 2;
    bool sawGraphics = false;
    bool sawGlyph = false;
    for (int y = cy - 30; y <= cy + 30; ++y) {
        for (int x = cx - 30; x <= cx + 30; ++x) {
            if (x < 0 || y < 0 || x >= shot.width() || y >= shot.height())
                continue;
            const QColor colour = shot.pixelColor(x, y);
            if (colour == QColor(255, 0, 0))
                sawGraphics = true;
            else if (colour.green() > 100 || colour.blue() > 100)
                sawGlyph = true;  // neither the picture nor a blend of it with black
        }
    }
    // The picture must be present and the glyph legible on top of it. With the
    // picture composited over the cells instead, the glyph was buried.
    QVERIFY(sawGraphics);
    QVERIFY(sawGlyph);
}

void TestCommandHandlerSoh::testGddmScreenCopyEmitsRequestAndWritesNothing() {
    QList<QByteArray> responses;
    m_handler->setSendToHostCallback(
        [&responses](const QByteArray &response) { responses.append(response); });
    QSignalSpy spy(m_handler, &TN5250CommandHandler::screenCopyRequested);

    // Order C1. The device would print; we report the request and write
    // nothing, so a host cannot use it as a file-write primitive.
    m_handler->handleRawScreenData(QByteArray::fromHex("ff93c195"));

    QCOMPARE(spy.count(), 1);
    // The block is still acknowledged normally.
    QCOMPARE(responses.size(), 1);
    QCOMPARE(static_cast<uint8_t>(responses[0][2]), static_cast<uint8_t>(0x3C));
}

void TestCommandHandlerSoh::testCompositeExportContainsBothPlanes() {
    const int glyphRow = m_widget->screenBuffer()->rows() / 2;
    const int glyphCol = m_widget->screenBuffer()->cols() / 2;
    m_widget->screenBuffer()->writeChar(glyphRow, glyphCol, 0xC1); // EBCDIC 'A'
    m_handler->handleRawScreenData(QByteArray::fromHex("ff93a34195"));
    m_widget->resize(720, 480);

    const QString path =
        QDir::temp().filePath(QStringLiteral("5250ng_composite_test.png"));
    QFile::remove(path);
    QVERIFY(m_widget->exportCompositeScreen(path));

    // The export must carry the composite, not one plane: the graphics plane is
    // only ever combined with the text here, which is the whole reason a local
    // screen copy has to be the client's job.
    QImage written(path);
    QVERIFY(!written.isNull());
    QCOMPARE(written.size(), m_widget->size());

    const int cx = written.width() / 2;
    const int cy = written.height() / 2;
    bool sawGraphics = false;
    bool sawGlyph = false;
    for (int y = cy - 30; y <= cy + 30; ++y) {
        for (int x = cx - 30; x <= cx + 30; ++x) {
            if (x < 0 || y < 0 || x >= written.width() || y >= written.height())
                continue;
            const QColor colour = written.pixelColor(x, y);
            if (colour == QColor(255, 0, 0))
                sawGraphics = true;
            else if (colour.green() > 100 || colour.blue() > 100)
                sawGlyph = true;
        }
    }
    QVERIFY(sawGraphics);
    QVERIFY(sawGlyph);
    QFile::remove(path);
}

QTEST_MAIN(TestCommandHandlerSoh)
#include "test_command_handler_soh.moc"
