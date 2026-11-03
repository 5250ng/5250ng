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

#include "core/keyboard_mapping.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QtTest/QtTest>

using core::MappedAction;
using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// The virtual keyboard and hotspots call Q5250ScreenWidget::acceptsInput()
// before dispatching; it must apply the same rules as typed keys.
class TestInputGate : public QObject {
    Q_OBJECT

  private slots:
    void gate_data() {
        QTest::addColumn<int>("state");
        QTest::addColumn<bool>("readOnly");
        QTest::addColumn<int>("action");
        QTest::addColumn<bool>("accepted");
        const int U = int(KeyboardState::Unlocked), L = int(KeyboardState::Locked),
                  E = int(KeyboardState::ErrorLocked);
        QTest::newRow("unlocked PF3") << U << false << int(MappedAction::PF3) << true;
        QTest::newRow("unlocked char") << U << false << int(MappedAction::None) << true;
        QTest::newRow("locked Enter") << L << false << int(MappedAction::Enter) << false;
        QTest::newRow("locked PF3") << L << false << int(MappedAction::PF3) << false;
        QTest::newRow("locked char") << L << false << int(MappedAction::None) << false;
        QTest::newRow("locked Attn") << L << false << int(MappedAction::Attn) << true;
        QTest::newRow("locked SysReq") << L << false << int(MappedAction::SysReq) << true;
        QTest::newRow("error Enter") << E << false << int(MappedAction::Enter) << false;
        QTest::newRow("error Reset") << E << false << int(MappedAction::Reset) << true;
        QTest::newRow("error Help") << E << false << int(MappedAction::Help) << true;
        const int S = int(KeyboardState::SystemRequest);
        QTest::newRow("locked arrow") << L << false << int(MappedAction::ArrowLeft) << false;
        QTest::newRow("error Tab") << E << false << int(MappedAction::Tab) << false;
        QTest::newRow("sysreq Enter") << S << false << int(MappedAction::Enter) << true;
        QTest::newRow("read-only PF3") << U << true << int(MappedAction::PF3) << false;
        QTest::newRow("read-only char") << U << true << int(MappedAction::None) << false;
        QTest::newRow("read-only Attn") << U << true << int(MappedAction::Attn) << false;
    }

    void gate() {
        QFETCH(int, state);
        QFETCH(bool, readOnly);
        QFETCH(int, action);
        QFETCH(bool, accepted);
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setKeyboardState(static_cast<KeyboardState>(state));
        widget.setReadOnly(readOnly);
        QCOMPARE(widget.acceptsInput(static_cast<MappedAction>(action)), accepted);
    }

    // MCP drives read-only sessions through injected input
    void mcpInjectionPassesReadOnly() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setKeyboardState(KeyboardState::Unlocked);
        widget.setReadOnly(true);
        widget.setMcpInjecting(true);
        QVERIFY(widget.acceptsInput(MappedAction::Enter));
    }
};

QTEST_MAIN(TestInputGate)
#include "test_input_gate.moc"
