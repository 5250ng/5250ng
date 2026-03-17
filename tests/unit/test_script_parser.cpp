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

#include "core/scripting/script_parser.h"
#include <QtTest/QtTest>

using namespace core::scripting;

class TestScriptParser : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testEmptyScript();
    void testComments();
    void testTypeInput();
    void testPressKey();
    void testAIDKeys();
    void testLocalKeys();
    void testMoveCursorAtPosition();
    void testMoveCursorDirection();
    void testMoveCursorAtInputField();
    void testMoveCursorAtNextPrevious();
    void testExpectTextAnywhere();
    void testExpectTextAtPos();
    void testExpectTextAtRow();
    void testExpectCursorAtPos();
    void testExpectKeyboardUnlocked();
    void testExpectNot();
    void testExpectMessageWaiting();
    void testExtractFromPosition();
    void testExtractField();
    void testExtractCursorRow();
    void testExtractCursorCol();
    void testWait();
    void testGlobalExpectTimeout();
    void testGlobalDelay();
    void testGlobalJitter();
    void testSetIncDecAdd();
    void testIfElseEndif();
    void testWhileEndwhile();
    void testRepeatEndrepeat();
    void testLabelGoto();
    void testOnTimeoutGoto();
    void testOnErrorGoto();
    void testAbort();
    void testLog();
    void testPause();
    void testNestedBlocks();
    void testUnclosedBlock();
    void testMismatchedBlock();
    void testDuplicateLabel();
    void testMissingGotoTarget();
    void testFullLoginScript();
    void testFunctionDefAndCall();
    void testFunctionZeroArgs();
    void testFunctionReturn();
    void testNestedDefError();
    void testDefInsideBlockError();
    void testCallUndefinedFunction();
    void testCallWrongArgCount();
    void testDuplicateFunction();
    void testFunctionExtractedFromRoot();
    void testFunctionWithLoginScript();
    void testBareCallSyntax();
    void testExtractLine();
    void testIfContains();
    void testInput();
    void testInputMultiple();

  private:
    ScriptParser *m_parser;
};

void TestScriptParser::init() {
    m_parser = new ScriptParser();
}

void TestScriptParser::cleanup() {
    delete m_parser;
    m_parser = nullptr;
}

void TestScriptParser::testEmptyScript() {
    auto result = m_parser->parse("");
    QVERIFY(!result.hasErrors());
    QVERIFY(result.root->children.isEmpty());
}

void TestScriptParser::testComments() {
    auto result = m_parser->parse("# A comment\n# Another comment\nENTER");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 1);
    QCOMPARE(result.root->children[0]->type, NodeType::AIDKey);
}

void TestScriptParser::testTypeInput() {
    auto result = m_parser->parse("TYPE \"Hello World\"");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::TypeInput);
    QCOMPARE(result.root->children[0]->stringValue, "Hello World");
}

void TestScriptParser::testPressKey() {
    // PRESS with AID key
    auto result = m_parser->parse("PRESS ENTER");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::AIDKey);
    QCOMPARE(node->aidByte, static_cast<uint8_t>(0xF1));

    // PRESS with function key
    result = m_parser->parse("PRESS F12");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->type, NodeType::AIDKey);
    QCOMPARE(node->aidByte, static_cast<uint8_t>(0x3C));

    // PRESS with local key
    result = m_parser->parse("PRESS TAB");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->type, NodeType::LocalKey);
    QCOMPARE(node->stringValue, "TAB");

    // PRESS with ESC
    result = m_parser->parse("PRESS ESC");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->type, NodeType::LocalKey);
    QCOMPARE(node->stringValue, "ESC");

    // PRESS with KEY_* character constant
    result = m_parser->parse("PRESS KEY_A");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->type, NodeType::CharInput);
    QCOMPARE(node->stringValue, "A");

    // PRESS with KEY_SPACE
    result = m_parser->parse("PRESS KEY_SPACE");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->type, NodeType::CharInput);
    QCOMPARE(node->stringValue, " ");
}

void TestScriptParser::testAIDKeys() {
    auto result = m_parser->parse("ENTER\nF1\nF12\nPAGEUP");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 4);
    for (const auto &child : result.root->children)
        QCOMPARE(child->type, NodeType::AIDKey);
    QCOMPARE(result.root->children[0]->aidByte, static_cast<uint8_t>(0xF1));
    QCOMPARE(result.root->children[1]->aidByte, static_cast<uint8_t>(0x31));
    QCOMPARE(result.root->children[2]->aidByte, static_cast<uint8_t>(0x3C));
    QCOMPARE(result.root->children[3]->aidByte, static_cast<uint8_t>(0xF5));
}

void TestScriptParser::testLocalKeys() {
    auto result = m_parser->parse("TAB\nBACKTAB\nBACKSPACE");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 3);
    for (const auto &child : result.root->children)
        QCOMPARE(child->type, NodeType::LocalKey);
}

void TestScriptParser::testMoveCursorAtPosition() {
    auto result = m_parser->parse("MOVE CURSOR AT (5,20)");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::MoveCursorAt);
    QCOMPARE(node->moveCursorMode, MoveCursorMode::Position);
    QCOMPARE(node->intValue, 5);
    QCOMPARE(node->intValue2, 20);
}

void TestScriptParser::testMoveCursorDirection() {
    // With explicit count
    auto result = m_parser->parse("MOVE CURSOR UP 3");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::MoveCursorAt);
    QCOMPARE(node->moveCursorMode, MoveCursorMode::StepUp);
    QCOMPARE(node->intValue, 3);

    // Without count (defaults to 1)
    result = m_parser->parse("MOVE CURSOR DOWN");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->moveCursorMode, MoveCursorMode::StepDown);
    QCOMPARE(node->intValue, 1);

    // Left and right
    result = m_parser->parse("MOVE CURSOR LEFT 5");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->moveCursorMode, MoveCursorMode::StepLeft);
    QCOMPARE(result.root->children[0]->intValue, 5);

    result = m_parser->parse("MOVE CURSOR RIGHT 1");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->moveCursorMode, MoveCursorMode::StepRight);
    QCOMPARE(result.root->children[0]->intValue, 1);
}

void TestScriptParser::testMoveCursorAtInputField() {
    auto result = m_parser->parse("MOVE CURSOR AT INPUTFIELD 2");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::MoveCursorAt);
    QCOMPARE(node->moveCursorMode, MoveCursorMode::FieldIndex);
    QCOMPARE(node->intValue, 2);
}

void TestScriptParser::testMoveCursorAtNextPrevious() {
    auto result = m_parser->parse("MOVE CURSOR AT NEXT INPUTFIELD");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::MoveCursorAt);
    QCOMPARE(node->moveCursorMode, MoveCursorMode::NextField);

    result = m_parser->parse("MOVE CURSOR AT PREVIOUS INPUTFIELD");
    QVERIFY(!result.hasErrors());
    node = result.root->children[0];
    QCOMPARE(node->type, NodeType::MoveCursorAt);
    QCOMPARE(node->moveCursorMode, MoveCursorMode::PreviousField);
}

void TestScriptParser::testExpectTextAnywhere() {
    auto result = m_parser->parse("EXPECT TEXT \"Sign On\"");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::Expect);
    QCOMPARE(node->expectType, ExpectType::TextAnywhere);
    QCOMPARE(node->stringValue, "Sign On");
    QVERIFY(!node->negated);
}

void TestScriptParser::testExpectTextAtPos() {
    auto result = m_parser->parse("EXPECT TEXT \"Sign On\" AT 1 23");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->expectType, ExpectType::TextAtPos);
    QCOMPARE(node->intValue, 1);
    QCOMPARE(node->intValue2, 23);
}

void TestScriptParser::testExpectTextAtRow() {
    auto result = m_parser->parse("EXPECT TEXT \"Sign On\" AT ROW 1");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->expectType, ExpectType::TextAtRow);
    QCOMPARE(node->intValue, 1);
}

void TestScriptParser::testExpectCursorAtPos() {
    auto result = m_parser->parse("EXPECT CURSOR AT 6 20");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->expectType, ExpectType::CursorAtPos);
    QCOMPARE(node->intValue, 6);
    QCOMPARE(node->intValue2, 20);
}

void TestScriptParser::testExpectKeyboardUnlocked() {
    auto result = m_parser->parse("EXPECT KEYBOARD UNLOCKED");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->expectType, ExpectType::KeyboardUnlocked);
}

void TestScriptParser::testExpectNot() {
    auto result = m_parser->parse("EXPECT NOT TEXT \"Error\"");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->expectType, ExpectType::TextAnywhere);
    QVERIFY(node->negated);
    QCOMPARE(node->stringValue, "Error");
}

void TestScriptParser::testExpectMessageWaiting() {
    auto result = m_parser->parse("EXPECT MESSAGEWAITING");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->expectType, ExpectType::MessageWaiting);
}

void TestScriptParser::testExtractFromPosition() {
    auto result = m_parser->parse("EXTRACT $TITLE FROM 1 1 LENGTH 40");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::Extract);
    QCOMPARE(node->extractType, ExtractType::FromPosition);
    QCOMPARE(node->varName, "$TITLE");
    QCOMPARE(node->intValue, 1);
    QCOMPARE(node->intValue2, 1);
    QCOMPARE(node->stringValue, "40");
}

void TestScriptParser::testExtractField() {
    auto result = m_parser->parse("EXTRACT $VAL FIELD AT 6 20");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->extractType, ExtractType::FieldAt);
}

void TestScriptParser::testExtractCursorRow() {
    auto result = m_parser->parse("EXTRACT $ROW CURSOR ROW");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->extractType, ExtractType::CursorRow);
}

void TestScriptParser::testExtractCursorCol() {
    auto result = m_parser->parse("EXTRACT $COL CURSOR COL");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::Extract);
    QCOMPARE(node->extractType, ExtractType::CursorCol);
    QCOMPARE(node->varName, "$COL");
}

void TestScriptParser::testWait() {
    auto result = m_parser->parse("WAIT 2000");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::Wait);
    QCOMPARE(result.root->children[0]->intValue, 2000);
}

void TestScriptParser::testGlobalExpectTimeout() {
    auto result = m_parser->parse("GLOBAL EXPECT_TIMEOUT 10000");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::GlobalExpectTimeout);
    QCOMPARE(result.root->children[0]->intValue, 10000);
}

void TestScriptParser::testGlobalDelay() {
    auto result = m_parser->parse("GLOBAL DELAY 100");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::GlobalDelay);
    QCOMPARE(result.root->children[0]->intValue, 100);

    // GLOBAL DELAY 0 resets
    result = m_parser->parse("GLOBAL DELAY 0");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->intValue, 0);
}

void TestScriptParser::testGlobalJitter() {
    auto result = m_parser->parse("GLOBAL JITTER 50 200");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::GlobalJitter);
    QCOMPARE(node->intValue, 50);
    QCOMPARE(node->intValue2, 200);

    // GLOBAL JITTER 0 0 disables
    result = m_parser->parse("GLOBAL JITTER 0 0");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->intValue, 0);
    QCOMPARE(result.root->children[0]->intValue2, 0);

    // Max clamped to min if reversed
    result = m_parser->parse("GLOBAL JITTER 200 50");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->intValue, 200);
    QCOMPARE(result.root->children[0]->intValue2, 200);
}

void TestScriptParser::testSetIncDecAdd() {
    auto result = m_parser->parse("SET $X \"hello\"\nINC $X\nDEC $X\nADD $X 5");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::Set);
    QCOMPARE(result.root->children[0]->varName, "$X");
    QCOMPARE(result.root->children[0]->stringValue, "hello");
    QCOMPARE(result.root->children[1]->type, NodeType::Inc);
    QCOMPARE(result.root->children[2]->type, NodeType::Dec);
    QCOMPARE(result.root->children[3]->type, NodeType::Add);
    QCOMPARE(result.root->children[3]->intValue, 5);
}

void TestScriptParser::testIfElseEndif() {
    auto result = m_parser->parse(
        "IF $X == \"OK\"\n"
        "    LOG \"success\"\n"
        "ELSE\n"
        "    LOG \"fail\"\n"
        "ENDIF\n"
    );
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 1);
    auto ifNode = result.root->children[0];
    QCOMPARE(ifNode->type, NodeType::If);
    QCOMPARE(ifNode->condLeft, "$X");
    QCOMPARE(ifNode->condOp, CompareOp::Eq);
    QCOMPARE(ifNode->condRight, "OK");
    QCOMPARE(ifNode->children.size(), 1);
    QCOMPARE(ifNode->elseChildren.size(), 1);
}

void TestScriptParser::testWhileEndwhile() {
    auto result = m_parser->parse(
        "WHILE $I < 3\n"
        "    INC $I\n"
        "ENDWHILE\n"
    );
    QVERIFY(!result.hasErrors());
    auto whileNode = result.root->children[0];
    QCOMPARE(whileNode->type, NodeType::While);
    QCOMPARE(whileNode->children.size(), 1);
}

void TestScriptParser::testRepeatEndrepeat() {
    auto result = m_parser->parse(
        "REPEAT 3\n"
        "    ENTER\n"
        "ENDREPEAT\n"
    );
    QVERIFY(!result.hasErrors());
    auto repeatNode = result.root->children[0];
    QCOMPARE(repeatNode->type, NodeType::Repeat);
    QCOMPARE(repeatNode->intValue, 3);
    QCOMPARE(repeatNode->children.size(), 1);
}

void TestScriptParser::testLabelGoto() {
    auto result = m_parser->parse("LABEL start\nENTER\nGOTO start");
    QVERIFY(!result.hasErrors());
    QVERIFY(result.labels.contains("start"));
    QCOMPARE(result.labels["start"], 0);
}

void TestScriptParser::testOnTimeoutGoto() {
    auto result = m_parser->parse("LABEL handler\nON TIMEOUT GOTO handler");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[1]->type, NodeType::OnTimeout);
    QCOMPARE(result.root->children[1]->stringValue, "handler");
}

void TestScriptParser::testOnErrorGoto() {
    auto result = m_parser->parse("LABEL handler\nON ERROR GOTO handler");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[1]->type, NodeType::OnError);
}

void TestScriptParser::testAbort() {
    auto result = m_parser->parse("ABORT \"Login failed\"");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::Abort);
    QCOMPARE(result.root->children[0]->stringValue, "Login failed");
}

void TestScriptParser::testLog() {
    auto result = m_parser->parse("LOG \"Hello World\"");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::Log);
    QCOMPARE(result.root->children[0]->stringValue, "Hello World");
}

void TestScriptParser::testPause() {
    auto result = m_parser->parse("PAUSE");
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children[0]->type, NodeType::Pause);
}

void TestScriptParser::testNestedBlocks() {
    auto result = m_parser->parse(
        "IF $X == 1\n"
        "    IF $Y == 2\n"
        "        LOG \"nested\"\n"
        "    ENDIF\n"
        "ENDIF\n"
    );
    QVERIFY(!result.hasErrors());
    auto outerIf = result.root->children[0];
    QCOMPARE(outerIf->children.size(), 1);
    auto innerIf = outerIf->children[0];
    QCOMPARE(innerIf->type, NodeType::If);
    QCOMPARE(innerIf->children.size(), 1);
}

void TestScriptParser::testUnclosedBlock() {
    auto result = m_parser->parse("IF $X == 1\nLOG \"no endif\"");
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("Unclosed IF")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testMismatchedBlock() {
    auto result = m_parser->parse("IF $X == 1\nENDWHILE");
    QVERIFY(result.hasErrors());
}

void TestScriptParser::testDuplicateLabel() {
    auto result = m_parser->parse("LABEL foo\nLABEL foo");
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("Duplicate label")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testMissingGotoTarget() {
    auto result = m_parser->parse("GOTO nonexistent");
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("not found")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testFullLoginScript() {
    QString script =
        "# AS/400 Login\n"
        "GLOBAL EXPECT_TIMEOUT 15000\n"
        "SET $RETRIES 0\n"
        "\n"
        "LABEL login\n"
        "EXPECT TEXT \"Sign On\" AT 1 23\n"
        "MOVE CURSOR AT (6,53)\n"
        "TYPE \"QSECOFR\"\n"
        "MOVE CURSOR AT (7,53)\n"
        "TYPE \"mypassword\"\n"
        "ENTER\n"
        "\n"
        "EXPECT TEXT \"MAIN MENU\" AT 1 30\n"
        "IF $EXPECT_RESULT == \"TIMEOUT\"\n"
        "    INC $RETRIES\n"
        "    IF $RETRIES < 3\n"
        "        LOG \"Retry $RETRIES\"\n"
        "        GOTO login\n"
        "    ENDIF\n"
        "    ABORT \"Login failed after 3 retries\"\n"
        "ENDIF\n"
        "LOG \"Login successful\"\n";

    auto result = m_parser->parse(script);
    QVERIFY(!result.hasErrors());
    QVERIFY(result.labels.contains("login"));
    QVERIFY(result.root->children.size() > 5);
}

void TestScriptParser::testFunctionDefAndCall() {
    auto result = m_parser->parse(
        "DEF login($user, $pass)\n"
        "    TYPE \"$user\"\n"
        "    MOVE CURSOR AT NEXT INPUTFIELD\n"
        "    TYPE \"$pass\"\n"
        "    ENTER\n"
        "ENDDEF\n"
        "\n"
        "CALL login(\"admin\", \"secret123\")\n"
    );
    QVERIFY(!result.hasErrors());
    // Function should be extracted from root
    QVERIFY(result.functions.contains("login"));
    auto func = result.functions["login"];
    QCOMPARE(func->type, NodeType::FunctionDef);
    QCOMPARE(func->stringValue, "login");
    QCOMPARE(func->paramNames.size(), 2);
    QCOMPARE(func->paramNames[0], "$user");
    QCOMPARE(func->paramNames[1], "$pass");
    QCOMPARE(func->children.size(), 4);

    // Root should only contain the CALL
    QCOMPARE(result.root->children.size(), 1);
    auto call = result.root->children[0];
    QCOMPARE(call->type, NodeType::FunctionCall);
    QCOMPARE(call->stringValue, "login");
    QCOMPARE(call->argValues.size(), 2);
    QCOMPARE(call->argValues[0], "admin");
    QCOMPARE(call->argValues[1], "secret123");
}

void TestScriptParser::testFunctionZeroArgs() {
    auto result = m_parser->parse(
        "DEF doNothing()\n"
        "    LOG \"nothing\"\n"
        "ENDDEF\n"
        "CALL doNothing()\n"
    );
    QVERIFY(!result.hasErrors());
    QVERIFY(result.functions.contains("doNothing"));
    QCOMPARE(result.functions["doNothing"]->paramNames.size(), 0);
    QCOMPARE(result.root->children[0]->argValues.size(), 0);
}

void TestScriptParser::testFunctionReturn() {
    auto result = m_parser->parse(
        "DEF earlyReturn($x)\n"
        "    IF $x == \"done\"\n"
        "        RETURN\n"
        "    ENDIF\n"
        "    LOG \"still going\"\n"
        "ENDDEF\n"
        "CALL earlyReturn(\"done\")\n"
    );
    QVERIFY(!result.hasErrors());
    auto func = result.functions["earlyReturn"];
    // IF node + LOG node
    QCOMPARE(func->children.size(), 2);
    // IF body should have RETURN
    auto ifNode = func->children[0];
    QCOMPARE(ifNode->type, NodeType::If);
    QCOMPARE(ifNode->children.size(), 1);
    QCOMPARE(ifNode->children[0]->type, NodeType::Return);
}

void TestScriptParser::testNestedDefError() {
    auto result = m_parser->parse(
        "DEF outer()\n"
        "    DEF inner()\n"
        "        LOG \"nested\"\n"
        "    ENDDEF\n"
        "ENDDEF\n"
    );
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("DEF must be at the top level")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testDefInsideBlockError() {
    auto result = m_parser->parse(
        "IF $X == 1\n"
        "    DEF bad()\n"
        "        LOG \"inside if\"\n"
        "    ENDDEF\n"
        "ENDIF\n"
    );
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("DEF must be at the top level")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testCallUndefinedFunction() {
    auto result = m_parser->parse("CALL nonexistent()");
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("CALL target") && e.message.contains("not found")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testCallWrongArgCount() {
    auto result = m_parser->parse(
        "DEF greet($name)\n"
        "    LOG $name\n"
        "ENDDEF\n"
        "CALL greet(\"a\", \"b\")\n"
    );
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("expects 1 arguments, got 2")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testDuplicateFunction() {
    auto result = m_parser->parse(
        "DEF foo()\n"
        "    LOG \"first\"\n"
        "ENDDEF\n"
        "DEF foo()\n"
        "    LOG \"second\"\n"
        "ENDDEF\n"
    );
    QVERIFY(result.hasErrors());
    bool found = false;
    for (const auto &e : result.errors) {
        if (e.message.contains("Duplicate function")) found = true;
    }
    QVERIFY(found);
}

void TestScriptParser::testFunctionExtractedFromRoot() {
    auto result = m_parser->parse(
        "LABEL start\n"
        "DEF myFunc()\n"
        "    LOG \"in func\"\n"
        "ENDDEF\n"
        "LOG \"main\"\n"
        "CALL myFunc()\n"
    );
    QVERIFY(!result.hasErrors());
    // Root should have: LABEL, LOG, CALL (no FunctionDef)
    QCOMPARE(result.root->children.size(), 3);
    QCOMPARE(result.root->children[0]->type, NodeType::Label);
    QCOMPARE(result.root->children[1]->type, NodeType::Log);
    QCOMPARE(result.root->children[2]->type, NodeType::FunctionCall);
    // Label index should be correct after extraction
    QVERIFY(result.labels.contains("start"));
    QCOMPARE(result.labels["start"], 0);
}

void TestScriptParser::testFunctionWithLoginScript() {
    QString script =
        "DEF login($user, $pass)\n"
        "    TYPE \"$user\"\n"
        "    MOVE CURSOR AT NEXT INPUTFIELD\n"
        "    TYPE \"$pass\"\n"
        "    PRESS ENTER\n"
        "ENDDEF\n"
        "\n"
        "EXPECT TEXT \"Sign On\"\n"
        "CALL login(\"admin\", \"secret123\")\n"
        "CALL login($saved_user, $saved_pass)\n";

    auto result = m_parser->parse(script);
    QVERIFY(!result.hasErrors());
    QVERIFY(result.functions.contains("login"));
    QCOMPARE(result.root->children.size(), 3); // EXPECT + 2 CALLs
    // Second call uses variables
    auto call2 = result.root->children[2];
    QCOMPARE(call2->argValues[0], "$saved_user");
    QCOMPARE(call2->argValues[1], "$saved_pass");
}

void TestScriptParser::testBareCallSyntax() {
    // Bare call without CALL keyword
    auto result = m_parser->parse(
        "DEF greet($name)\n"
        "    LOG \"$name\"\n"
        "ENDDEF\n"
        "greet(\"World\")\n"
    );
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 1);
    auto call = result.root->children[0];
    QCOMPARE(call->type, NodeType::FunctionCall);
    QCOMPARE(call->stringValue, "greet");
    QCOMPARE(call->argValues.size(), 1);
    QCOMPARE(call->argValues[0], "World");

    // Bare call with variables
    result = m_parser->parse(
        "DEF login($user, $pass)\n"
        "    LOG \"$user\"\n"
        "ENDDEF\n"
        "login(\"QSECOFR\", \"QSECOFR0\")\n"
    );
    QVERIFY(!result.hasErrors());
    auto call2 = result.root->children[0];
    QCOMPARE(call2->type, NodeType::FunctionCall);
    QCOMPARE(call2->stringValue, "login");
    QCOMPARE(call2->argValues.size(), 2);
    QCOMPARE(call2->argValues[0], "QSECOFR");
    QCOMPARE(call2->argValues[1], "QSECOFR0");

    // Both CALL and bare syntax work
    result = m_parser->parse(
        "DEF foo()\n"
        "    LOG \"foo\"\n"
        "ENDDEF\n"
        "CALL foo()\n"
        "foo()\n"
    );
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 2);
    QCOMPARE(result.root->children[0]->type, NodeType::FunctionCall);
    QCOMPARE(result.root->children[1]->type, NodeType::FunctionCall);
}

void TestScriptParser::testExtractLine() {
    auto result = m_parser->parse("EXTRACT $line LINE 5");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::Extract);
    QCOMPARE(node->extractType, ExtractType::LineAt);
    QCOMPARE(node->varName, "$line");
    QCOMPARE(node->intValue, 5);
}

void TestScriptParser::testIfContains() {
    // IF with CONTAINS
    auto result = m_parser->parse(
        "IF $line CONTAINS \"Poda\"\n"
        "    LOG \"found it\"\n"
        "ENDIF\n"
    );
    QVERIFY(!result.hasErrors());
    auto ifNode = result.root->children[0];
    QCOMPARE(ifNode->type, NodeType::If);
    QCOMPARE(ifNode->condLeft, "$line");
    QCOMPARE(ifNode->condOp, CompareOp::Contains);
    QCOMPARE(ifNode->condRight, "Poda");
    QCOMPARE(ifNode->children.size(), 1);

    // WHILE with CONTAINS
    result = m_parser->parse(
        "WHILE $text CONTAINS \"error\"\n"
        "    LOG \"still has error\"\n"
        "ENDWHILE\n"
    );
    QVERIFY(!result.hasErrors());
    auto whileNode = result.root->children[0];
    QCOMPARE(whileNode->condOp, CompareOp::Contains);
}

void TestScriptParser::testInput() {
    auto result = m_parser->parse("INPUT \"Username:\" $USER");
    QVERIFY(!result.hasErrors());
    auto node = result.root->children[0];
    QCOMPARE(node->type, NodeType::Input);
    QCOMPARE(node->stringValue, "Username:");
    QCOMPARE(node->varName, "$USER");
}

void TestScriptParser::testInputMultiple() {
    auto result = m_parser->parse(
        "INPUT \"Username:\" $USER\n"
        "INPUT \"Password:\" $PASS\n"
        "INPUT \"Library:\" $LIB\n"
    );
    QVERIFY(!result.hasErrors());
    QCOMPARE(result.root->children.size(), 3);
    for (int i = 0; i < 3; ++i)
        QCOMPARE(result.root->children[i]->type, NodeType::Input);
    QCOMPARE(result.root->children[0]->stringValue, "Username:");
    QCOMPARE(result.root->children[0]->varName, "$USER");
    QCOMPARE(result.root->children[1]->stringValue, "Password:");
    QCOMPARE(result.root->children[1]->varName, "$PASS");
    QCOMPARE(result.root->children[2]->stringValue, "Library:");
    QCOMPARE(result.root->children[2]->varName, "$LIB");
}

QTEST_MAIN(TestScriptParser)
#include "test_script_parser.moc"
