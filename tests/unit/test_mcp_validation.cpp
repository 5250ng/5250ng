// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "agent/tool_definitions.h"
#include "mcp/mcp_validation.h"
#include <QTest>

class TestMcpValidation : public QObject {
    Q_OBJECT

  private slots:
    void acceptsValidPorts() {
        quint16 port = 0;
        QVERIFY(mcp::parseTcpPort(QJsonValue(QJsonValue::Undefined), &port));
        QCOMPARE(port, quint16(23));
        QVERIFY(mcp::parseTcpPort(QJsonValue(1), &port));
        QCOMPARE(port, quint16(1));
        QVERIFY(mcp::parseTcpPort(QJsonValue(65535), &port));
        QCOMPARE(port, quint16(65535));
    }

    void rejectsInvalidPorts() {
        quint16 port = 23;
        QVERIFY(!mcp::parseTcpPort(QJsonValue(0), &port));
        QVERIFY(!mcp::parseTcpPort(QJsonValue(-1), &port));
        QVERIFY(!mcp::parseTcpPort(QJsonValue(65536), &port));
        QVERIFY(!mcp::parseTcpPort(QJsonValue(23.5), &port));
        QVERIFY(!mcp::parseTcpPort(QJsonValue(QStringLiteral("23")), &port));
        QVERIFY(!mcp::parseTcpPort(QJsonValue(QJsonValue::Null), &port));
    }

    void schemaPublishesPortBounds() {
        const QJsonObject schema = agent::toolCreateSessionSchema();
        const QJsonObject port = schema.value("properties").toObject()
                                     .value("port").toObject();
        QCOMPARE(port.value("minimum").toInt(), 1);
        QCOMPARE(port.value("maximum").toInt(), 65535);
    }

    void schemaPublishesGraphicsDeviceType() {
        const QJsonObject schema = agent::toolCreateSessionSchema();
        const QJsonObject deviceType = schema.value("properties").toObject()
                                           .value("deviceType").toObject();
        QCOMPARE(deviceType.value("type").toString(), QString("string"));
        QCOMPARE(deviceType.value("default").toString(), QString("IBM-3179-2"));
    }

    // Mirrors Q5250ScreenWidget::keyPressEvent: an unlocked keyboard takes
    // any input, so nothing is rejected.
    void unlockedKeyboardAcceptsInput() {
        using ui::widgets::KeyboardState;
        for (KeyboardState state : {KeyboardState::Unlocked, KeyboardState::SystemRequest}) {
            QVERIFY(mcp::lockedKeyboardError(state, QString()).isEmpty());
            QVERIFY(mcp::lockedKeyboardError(state, "ERASEEOF").isEmpty());
            QVERIFY(mcp::lockedKeyboardError(state, "ENTER").isEmpty());
        }
    }

    // Locked (waiting for the host): only Attn and SysReq reach the widget.
    void lockedKeyboardRejectsInput() {
        using ui::widgets::KeyboardState;
        const KeyboardState s = KeyboardState::Locked;
        QVERIFY(!mcp::lockedKeyboardError(s, QString()).isEmpty()); // typed text
        QVERIFY(!mcp::lockedKeyboardError(s, "ENTER").isEmpty());
        QVERIFY(!mcp::lockedKeyboardError(s, "ERASEINPUT").isEmpty());
        QVERIFY(!mcp::lockedKeyboardError(s, "ESC").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "ATTN").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "SYSREQ").isEmpty());
        // Arrow keys move the cursor directly, not through keyPressEvent.
        QVERIFY(mcp::lockedKeyboardError(s, "UP").isEmpty());
    }

    // Error-locked: only Error Reset (ESC), Attn and Help reach the widget,
    // and the message tells the client how to recover.
    void errorLockedKeyboardRejectsInput() {
        using ui::widgets::KeyboardState;
        const KeyboardState s = KeyboardState::ErrorLocked;
        const QString err = mcp::lockedKeyboardError(s, QString());
        QVERIFY(!err.isEmpty());
        QVERIFY(err.contains("ESC"));
        QVERIFY(!mcp::lockedKeyboardError(s, "ERASEEOF").isEmpty());
        QVERIFY(!mcp::lockedKeyboardError(s, "ENTER").isEmpty());
        QVERIFY(!mcp::lockedKeyboardError(s, "SYSREQ").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "ESC").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "ESCAPE").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "ATTN").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "HELP").isEmpty());
        QVERIFY(mcp::lockedKeyboardError(s, "LEFT").isEmpty());
    }
};

QTEST_MAIN(TestMcpValidation)
#include "test_mcp_validation.moc"
