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

#include "core/ebcdic.h"
#include "core/keyboard_mapping.h"
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
    void testClearUnitDoesNotReuseStaleInsertCursor();
    void testInsertCursorKeptWithoutInputFields();
    void testGddmBlockBypassesAlphanumericRendererAndPaces();
    void testGddmSuppressPacingOrder();
    void testGddmReadStatusWithAlphanumericPrefix();
    void testGddmStatusWriteIsIndependentOfPacing();
    void testGddmResetAndErrorUseDocumentedAids();
    void testEBCDICffInTextDoesNotBeginGraphics();

    void testGddmGraphicsPlaneRendersBehindText();
    void testGddmCanvasOpacityAffectsOnlyGraphicsPlane();
    void testGddmScreenCopyEmitsRequestAndWritesNothing();
    void testCompositeExportContainsBothPlanes();

    // Issue #213: 80/132 column switching driven by Clear Unit /
    // Clear Unit Alternate control characters.
    void testClearUnitAlternateSwitchesTo27x132();
    void testClearUnitSwitchesBackTo24x80();
    void testClearUnitFrom27x132ReturnsTo24x80();

    // Issue #249: the SOH command key mask marks PF keys whose AID is sent
    // without field data (command attention keys).
    void testMaskBitMapping();
    void testMaskedPfKeySendsNoFieldData();
    void testUnmaskedKeysSendFieldData();
    void testClearUnitResetsKeyMask();

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

void TestCommandHandlerSoh::testClearUnitDoesNotReuseStaleInsertCursor() {
    // The menu screen leaves its IC address on QSH's third input row. QSH
    // clears the unit and defines one wrapping field without sending a new IC
    // order, so unlock must choose that field's first position.
    m_widget->setICAddress(19, 6);
    m_handler->onClearScreenRequested();

    auto *screen = m_widget->screenBuffer();
    screen->setField(17, 6, 313, false);
    screen->setFieldFFW(17, 6, 0x48, 0x00);
    m_handler->processDeferredCC2(0x08);

    QCOMPARE(screen->cursorPosition(), QPoint(6, 17));

    // A real IC order on the new presentation space must still be honoured.
    m_widget->setICAddress(19, 6);
    m_handler->processDeferredCC2(0x08);
    QCOMPARE(screen->cursorPosition(), QPoint(6, 19));
}

void TestCommandHandlerSoh::testInsertCursorKeptWithoutInputFields() {
    // A screen with an IC order but no input field: unlock leaves the cursor
    // on the IC address, as it did before Clear Unit tracking existed.
    m_handler->onClearScreenRequested();
    m_widget->setICAddress(5, 10);
    m_handler->processDeferredCC2(0x08);
    QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(10, 5));
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

void TestCommandHandlerSoh::testGddmCanvasOpacityAffectsOnlyGraphicsPlane() {
    const int glyphRow = m_widget->screenBuffer()->rows() / 2;
    const int glyphCol = m_widget->screenBuffer()->cols() / 2;
    m_widget->screenBuffer()->writeChar(glyphRow, glyphCol, 0xC1); // EBCDIC 'A'
    m_handler->handleRawScreenData(QByteArray::fromHex("ff93a34195"));
    m_widget->setGddmGraphicsOpacity(0.5);
    QCOMPARE(m_widget->gddmGraphicsOpacity(), 0.5);

    m_widget->resize(720, 480);
    QImage shot(m_widget->size(), QImage::Format_ARGB32);
    shot.fill(Qt::black);
    m_widget->render(&shot);

    const int cx = shot.width() / 2;
    const int cy = shot.height() / 2;
    bool sawHalfOpacityGraphics = false;
    bool sawFullOpacityGlyph = false;
    for (int y = cy - 30; y <= cy + 30; ++y) {
        for (int x = cx - 30; x <= cx + 30; ++x) {
            const QColor colour = shot.pixelColor(x, y);
            if (colour.red() >= 110 && colour.red() <= 145
                && colour.green() < 10 && colour.blue() < 10) {
                sawHalfOpacityGraphics = true;
            }
            if (colour.green() > 200 || colour.blue() > 200) {
                sawFullOpacityGlyph = true;
            }
        }
    }
    QVERIFY(sawHalfOpacityGraphics);
    QVERIFY(sawFullOpacityGlyph);

    m_widget->setGddmGraphicsOpacity(-1.0);
    QCOMPARE(m_widget->gddmGraphicsOpacity(), 0.0);
    m_widget->setGddmGraphicsOpacity(2.0);
    QCOMPARE(m_widget->gddmGraphicsOpacity(), 1.0);
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

// Issue #213: the host switches a 3477-FC class display between 24x80 and
// 27x132 by sending Clear Unit (0x40) and Clear Unit Alternate (0x20) control
// characters. Both must drive the screen buffer geometry, not just clear it.

void TestCommandHandlerSoh::testClearUnitAlternateSwitchesTo27x132() {
    // Session starts 24x80; the host enters 132-column mode.
    m_widget->setScreenSize(24, 80);
    m_handler->onClearScreenAlternateRequested();
    QCOMPARE(m_widget->screenBuffer()->rows(), 27);
    QCOMPARE(m_widget->screenBuffer()->cols(), 132);
}

void TestCommandHandlerSoh::testClearUnitSwitchesBackTo24x80() {
    // After a 132-column screen, Clear Unit must return to 24x80
    // (tn5250j CMD_CLEAR_UNIT semantics; previously stayed at 27x132).
    m_widget->setScreenSize(27, 132);
    m_handler->onClearScreenRequested();
    QCOMPARE(m_widget->screenBuffer()->rows(), 24);
    QCOMPARE(m_widget->screenBuffer()->cols(), 80);
}

void TestCommandHandlerSoh::testClearUnitFrom27x132ReturnsTo24x80() {
    // Same round-trip as the issue reports: alternate (132) then clear (80).
    m_widget->setScreenSize(24, 80);
    m_handler->onClearScreenAlternateRequested();
    QCOMPARE(m_widget->screenBuffer()->rows(), 27);
    QCOMPARE(m_widget->screenBuffer()->cols(), 132);
    m_handler->onClearScreenRequested();
    QCOMPARE(m_widget->screenBuffer()->rows(), 24);
    QCOMPARE(m_widget->screenBuffer()->cols(), 80);
}

// lib5250 tn5250_dbuffer_send_data_for_aid_key: PF1 is bit 0x01 of the
// third mask byte, PF8 0x80, PF9 0x01 of the second, PF24 0x80 of the first
void TestCommandHandlerSoh::testMaskBitMapping() {
    struct Case { uint8_t aid; int byte; uint8_t bit; };
    const Case cases[] = {
        {0x31, 2, 0x01}, {0x33, 2, 0x04}, {0x38, 2, 0x80},  // PF1, PF3, PF8
        {0x39, 1, 0x01}, {0x3C, 1, 0x08},                   // PF9, PF12
        {0xB1, 1, 0x10}, {0xB4, 1, 0x80},                   // PF13, PF16
        {0xB5, 0, 0x01}, {0xBC, 0, 0x80},                   // PF17, PF24
    };
    for (const Case &c : cases) {
        uint8_t mask[3] = {0, 0, 0};
        QVERIFY(!Q5250ScreenWidget::aidSendsNoFieldData(mask, c.aid));
        mask[c.byte] = c.bit;
        QVERIFY2(Q5250ScreenWidget::aidSendsNoFieldData(mask, c.aid),
                 qPrintable(QString::number(c.aid, 16)));
        // No other PF key is affected by that bit
        for (uint8_t other : {uint8_t(0x31), uint8_t(0x38), uint8_t(0x39), uint8_t(0xB1),
                              uint8_t(0xB5), uint8_t(0xBC)}) {
            if (other != c.aid)
                QVERIFY(!Q5250ScreenWidget::aidSendsNoFieldData(mask, other));
        }
    }
    // Enter, Roll Up, Help and Clear always send data
    const uint8_t all[3] = {0xFF, 0xFF, 0xFF};
    for (uint8_t aid : {uint8_t(0xF1), uint8_t(0xF5), uint8_t(0xF3), uint8_t(0xBD)})
        QVERIFY(!Q5250ScreenWidget::aidSendsNoFieldData(all, aid));
}

namespace {
// A modified input field holding "AB" at row 5, column 10
void setUpModifiedField(Q5250ScreenWidget *widget) {
    auto *screen = widget->screenBuffer();
    screen->setField(4, 9, 5, false);
    screen->setFieldFFW(4, 9, 0x40, 0x00);
    screen->setCursorPosition(4, 9);
    widget->setKeyboardState(ui::widgets::KeyboardState::Unlocked);
    widget->dispatchCharacter('A');
    widget->dispatchCharacter('B');
}
} // namespace

void TestCommandHandlerSoh::testMaskedPfKeySendsNoFieldData() {
    setUpModifiedField(m_widget);
    // PF3 is a command attention key on this screen
    m_handler->onSohReceived(0, 0x00, 0x00, 0x04);
    QSignalSpy spy(m_widget, &Q5250ScreenWidget::inputReady);
    m_widget->dispatchMappedAction(core::MappedAction::PF3);
    QCOMPARE(spy.count(), 1);
    const QByteArray sent = spy.at(0).at(0).toByteArray();
    QCOMPARE(sent.size(), 3); // row, column, AID only
    QCOMPARE(quint8(sent[2]), quint8(0x33));
}

void TestCommandHandlerSoh::testUnmaskedKeysSendFieldData() {
    setUpModifiedField(m_widget);
    m_handler->onSohReceived(0, 0x00, 0x00, 0x04);
    QSignalSpy spy(m_widget, &Q5250ScreenWidget::inputReady);
    m_widget->dispatchMappedAction(core::MappedAction::PF4);
    QCOMPARE(spy.count(), 1);
    const QByteArray sent = spy.at(0).at(0).toByteArray();
    QCOMPARE(quint8(sent[2]), quint8(0x34));
    QVERIFY2(sent.contains(core::EBCDIC::stringToEBCDIC("AB")), sent.toHex().constData());
}

// Clear Unit clears the format table and its SOH header, so the mask too
void TestCommandHandlerSoh::testClearUnitResetsKeyMask() {
    m_handler->onSohReceived(0, 0x80, 0x80, 0x04);
    m_handler->onClearScreenRequested();
    const uint8_t *mask = m_widget->cmdKeyMask();
    QCOMPARE(int(mask[0]) | int(mask[1]) | int(mask[2]), 0);
    m_handler->onSohReceived(0, 0x00, 0x00, 0x04);
    m_handler->onClearFormatTableRequested();
    QCOMPARE(int(m_widget->cmdKeyMask()[2]), 0);
    m_handler->onSohReceived(0, 0x00, 0x00, 0x04);
    m_handler->onClearScreenAlternateRequested();
    QCOMPARE(int(m_widget->cmdKeyMask()[2]), 0);
}

QTEST_MAIN(TestCommandHandlerSoh)
#include "test_command_handler_soh.moc"
