#include "core/keyboard_encoder.h"
#include <QKeyEvent>
#include <QtTest/QtTest>

using namespace core;

class TestKeyboardEncoder : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testPFKeyEncoding();
    void testSpecialKeys();
    void testNormalCharacter();
    void testIsPFKey();
    void testGetPFKeyNumber();
    void testEnterKey();
    void testTabKeys();
    void testArrowKeys();

  private:
    KeyboardEncoder *m_encoder;
};

void TestKeyboardEncoder::init() { m_encoder = new KeyboardEncoder(this); }

void TestKeyboardEncoder::cleanup() {
    m_encoder->deleteLater();
    m_encoder = nullptr;
}

void TestKeyboardEncoder::testPFKeyEncoding() {
    // Test PF1-PF12
    for (int i = 1; i <= 12; ++i) {
        QByteArray encoded = m_encoder->encodePFKey(i);
        QVERIFY(!encoded.isEmpty());
        QCOMPARE(encoded.size(), 1);
    }

    // Test PF13-PF24
    for (int i = 13; i <= 24; ++i) {
        QByteArray encoded = m_encoder->encodePFKey(i);
        QVERIFY(!encoded.isEmpty());
        QCOMPARE(encoded.size(), 1);
    }

    // Test invalid PF key
    QByteArray invalid = m_encoder->encodePFKey(0);
    QVERIFY(invalid.isEmpty());

    invalid = m_encoder->encodePFKey(25);
    QVERIFY(invalid.isEmpty());
}

void TestKeyboardEncoder::testSpecialKeys() {
    // Test Enter
    QByteArray enter = m_encoder->encodeAction(KeyboardAction::Enter);
    QVERIFY(!enter.isEmpty());

    // Test Tab
    QByteArray tab = m_encoder->encodeAction(KeyboardAction::Tab);
    QVERIFY(!tab.isEmpty());

    // Test Clear
    QByteArray clear = m_encoder->encodeAction(KeyboardAction::Clear);
    QVERIFY(!clear.isEmpty());
}

void TestKeyboardEncoder::testNormalCharacter() {
    // Test encoding normal characters
    QByteArray a = m_encoder->encodeCharacter('A');
    QVERIFY(!a.isEmpty());
    QCOMPARE(a.size(), 1);

    QByteArray space = m_encoder->encodeCharacter(' ');
    QVERIFY(!space.isEmpty());
    QCOMPARE(space.size(), 1);
}

void TestKeyboardEncoder::testIsPFKey() {
    QVERIFY(KeyboardEncoder::isPFKey(Qt::Key_F1));
    QVERIFY(KeyboardEncoder::isPFKey(Qt::Key_F12));
    QVERIFY(KeyboardEncoder::isPFKey(Qt::Key_F24));
    QVERIFY(!KeyboardEncoder::isPFKey(Qt::Key_A));
    QVERIFY(!KeyboardEncoder::isPFKey(Qt::Key_Enter));
}

void TestKeyboardEncoder::testGetPFKeyNumber() {
    QCOMPARE(KeyboardEncoder::getPFKeyNumber(Qt::Key_F1), 1);
    QCOMPARE(KeyboardEncoder::getPFKeyNumber(Qt::Key_F12), 12);
    QCOMPARE(KeyboardEncoder::getPFKeyNumber(Qt::Key_F24), 24);
    QCOMPARE(KeyboardEncoder::getPFKeyNumber(Qt::Key_A), 0);
}

void TestKeyboardEncoder::testEnterKey() {
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QByteArray encoded = m_encoder->encodeKeyEvent(&event);
    QVERIFY(!encoded.isEmpty());
}

void TestKeyboardEncoder::testTabKeys() {
    // Regular Tab
    QKeyEvent tabEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QByteArray tab = m_encoder->encodeKeyEvent(&tabEvent);
    QVERIFY(!tab.isEmpty());

    // Shift+Tab (BackTab)
    QKeyEvent backTabEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier);
    QByteArray backTab = m_encoder->encodeKeyEvent(&backTabEvent);
    QVERIFY(!backTab.isEmpty());
}

void TestKeyboardEncoder::testArrowKeys() {
    QKeyEvent upEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
    QByteArray up = m_encoder->encodeKeyEvent(&upEvent);
    // Arrow keys may not have direct AID codes, but should not crash
    // The result depends on implementation

    QKeyEvent downEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QByteArray down = m_encoder->encodeKeyEvent(&downEvent);

    QKeyEvent leftEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    QByteArray left = m_encoder->encodeKeyEvent(&leftEvent);

    QKeyEvent rightEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QByteArray right = m_encoder->encodeKeyEvent(&rightEvent);
}

QTEST_MAIN(TestKeyboardEncoder)
#include "test_keyboard_encoder.moc"
