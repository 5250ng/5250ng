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

#include <5250script/script_lexer.h>
#include <QtTest/QtTest>

using namespace core::scripting;

class TestScriptLexer : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testEmptyInput();
    void testComment();
    void testTypeCommand();
    void testPressCommand();
    void testAIDKeys();
    void testLocalKeys();
    void testMoveCursorAtPosition();
    void testMoveCursorAtInputField();
    void testMoveCursorAtNextPrevious();
    void testMoveCursorDirection();
    void testExpectText();
    void testExpectKeyboard();
    void testWait();
    void testGlobalExpectTimeout();
    void testGlobalDelay();
    void testGlobalJitter();
    void testVariables();
    void testSetCommand();
    void testIfCondition();
    void testStringLiteralEscapes();
    void testNumberLiterals();
    void testComparisonOperators();
    void testMultilineScript();
    void testLabelGoto();
    void testOnTimeoutGoto();
    void testLogCommand();
    void testAbortCommand();
    void testRepeat();
    void testDefCallReturn();

  private:
    ScriptLexer *m_lexer;
};

void TestScriptLexer::init() {
    m_lexer = new ScriptLexer();
}

void TestScriptLexer::cleanup() {
    delete m_lexer;
    m_lexer = nullptr;
}

void TestScriptLexer::testEmptyInput() {
    auto result = m_lexer->tokenize("");
    QVERIFY(result.isEmpty());
}

void TestScriptLexer::testComment() {
    auto result = m_lexer->tokenize("# This is a comment");
    QVERIFY(result.isEmpty());

    // Inline comment
    result = m_lexer->tokenize("ENTER # send enter");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 1);
    QCOMPARE(result[0][0].type, TokenType::ENTER);
}

void TestScriptLexer::testTypeCommand() {
    auto result = m_lexer->tokenize("TYPE \"Hello World\"");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0].type, TokenType::TYPE);
    QCOMPARE(result[0][1].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][1].value, "Hello World");
}

void TestScriptLexer::testPressCommand() {
    // PRESS with a key name
    auto result = m_lexer->tokenize("PRESS ENTER");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0].type, TokenType::PRESS);
    QCOMPARE(result[0][1].type, TokenType::ENTER);

    // PRESS with a function key
    result = m_lexer->tokenize("PRESS F12");
    QCOMPARE(result[0][1].type, TokenType::F12);

    // PRESS with a local key
    result = m_lexer->tokenize("PRESS TAB");
    QCOMPARE(result[0][1].type, TokenType::TAB);

    // PRESS with ESC
    result = m_lexer->tokenize("PRESS ESC");
    QCOMPARE(result[0][1].type, TokenType::ESC);

    // PRESS with KEY_* character constants
    result = m_lexer->tokenize("PRESS KEY_A");
    QCOMPARE(result[0][1].type, TokenType::KEY_CHAR);
    QCOMPARE(result[0][1].value, "A");

    result = m_lexer->tokenize("PRESS KEY_5");
    QCOMPARE(result[0][1].type, TokenType::KEY_CHAR);
    QCOMPARE(result[0][1].value, "5");

    result = m_lexer->tokenize("PRESS KEY_SPACE");
    QCOMPARE(result[0][1].type, TokenType::KEY_CHAR);
    QCOMPARE(result[0][1].value, " ");

    result = m_lexer->tokenize("PRESS KEY_PERIOD");
    QCOMPARE(result[0][1].type, TokenType::KEY_CHAR);
    QCOMPARE(result[0][1].value, ".");
}

void TestScriptLexer::testAIDKeys() {
    auto result = m_lexer->tokenize("ENTER\nF1\nF12\nF24\nPAGEUP\nPAGEDOWN\nATTN\nSYSREQ\nHELP\nCLEAR\nPRINT");
    QCOMPARE(result.size(), 11);
    QCOMPARE(result[0][0].type, TokenType::ENTER);
    QCOMPARE(result[1][0].type, TokenType::F1);
    QCOMPARE(result[2][0].type, TokenType::F12);
    QCOMPARE(result[3][0].type, TokenType::F24);
    QCOMPARE(result[4][0].type, TokenType::PAGEUP);
    QCOMPARE(result[5][0].type, TokenType::PAGEDOWN);
    QCOMPARE(result[6][0].type, TokenType::ATTN);
    QCOMPARE(result[7][0].type, TokenType::SYSREQ);
    QCOMPARE(result[8][0].type, TokenType::HELP);
    QCOMPARE(result[9][0].type, TokenType::CLEAR);
    QCOMPARE(result[10][0].type, TokenType::PRINT);
}

void TestScriptLexer::testLocalKeys() {
    auto result = m_lexer->tokenize("TAB\nBACKTAB\nBACKSPACE\nDELETE\nINSERT\nHOME\nEND");
    QCOMPARE(result.size(), 7);
    QCOMPARE(result[0][0].type, TokenType::TAB);
    QCOMPARE(result[1][0].type, TokenType::BACKTAB);
    QCOMPARE(result[2][0].type, TokenType::BACKSPACE);
    QCOMPARE(result[3][0].type, TokenType::DELETE_KEY);
    QCOMPARE(result[4][0].type, TokenType::INSERT);
    QCOMPARE(result[5][0].type, TokenType::HOME);
    QCOMPARE(result[6][0].type, TokenType::END);
}

void TestScriptLexer::testMoveCursorAtPosition() {
    auto result = m_lexer->tokenize("MOVE CURSOR AT (5,20)");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 8);
    QCOMPARE(result[0][0].type, TokenType::MOVE);
    QCOMPARE(result[0][1].type, TokenType::CURSOR);
    QCOMPARE(result[0][2].type, TokenType::AT);
    QCOMPARE(result[0][3].type, TokenType::LPAREN);
    QCOMPARE(result[0][4].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][4].value, "5");
    QCOMPARE(result[0][5].type, TokenType::COMMA);
    QCOMPARE(result[0][6].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][6].value, "20");
    QCOMPARE(result[0][7].type, TokenType::RPAREN);

    // With spaces
    result = m_lexer->tokenize("MOVE CURSOR AT ( 5 , 20 )");
    QCOMPARE(result[0].size(), 8);
    QCOMPARE(result[0][4].value, "5");
    QCOMPARE(result[0][6].value, "20");
}

void TestScriptLexer::testMoveCursorAtInputField() {
    auto result = m_lexer->tokenize("MOVE CURSOR AT INPUTFIELD 3");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 5);
    QCOMPARE(result[0][0].type, TokenType::MOVE);
    QCOMPARE(result[0][1].type, TokenType::CURSOR);
    QCOMPARE(result[0][2].type, TokenType::AT);
    QCOMPARE(result[0][3].type, TokenType::INPUTFIELD);
    QCOMPARE(result[0][4].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][4].value, "3");
}

void TestScriptLexer::testMoveCursorAtNextPrevious() {
    auto result = m_lexer->tokenize("MOVE CURSOR AT NEXT INPUTFIELD");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 5);
    QCOMPARE(result[0][0].type, TokenType::MOVE);
    QCOMPARE(result[0][3].type, TokenType::NEXT);
    QCOMPARE(result[0][4].type, TokenType::INPUTFIELD);

    result = m_lexer->tokenize("MOVE CURSOR AT PREVIOUS INPUTFIELD");
    QCOMPARE(result[0].size(), 5);
    QCOMPARE(result[0][3].type, TokenType::PREVIOUS);
    QCOMPARE(result[0][4].type, TokenType::INPUTFIELD);
}

void TestScriptLexer::testMoveCursorDirection() {
    auto result = m_lexer->tokenize("MOVE CURSOR UP 2");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 4);
    QCOMPARE(result[0][0].type, TokenType::MOVE);
    QCOMPARE(result[0][1].type, TokenType::CURSOR);
    QCOMPARE(result[0][2].type, TokenType::UP);
    QCOMPARE(result[0][3].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][3].value, "2");

    // Without count
    result = m_lexer->tokenize("MOVE CURSOR DOWN");
    QCOMPARE(result[0].size(), 3);
    QCOMPARE(result[0][2].type, TokenType::DOWN);

    result = m_lexer->tokenize("MOVE CURSOR LEFT 5");
    QCOMPARE(result[0][2].type, TokenType::LEFT);

    result = m_lexer->tokenize("MOVE CURSOR RIGHT 1");
    QCOMPARE(result[0][2].type, TokenType::RIGHT);
}

void TestScriptLexer::testExpectText() {
    auto result = m_lexer->tokenize("EXPECT TEXT \"Sign On\" AT 1 23");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::EXPECT);
    QCOMPARE(result[0][1].type, TokenType::TEXT);
    QCOMPARE(result[0][2].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][2].value, "Sign On");
    QCOMPARE(result[0][3].type, TokenType::AT);
    QCOMPARE(result[0][4].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][4].value, "1");
    QCOMPARE(result[0][5].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][5].value, "23");
}

void TestScriptLexer::testExpectKeyboard() {
    auto result = m_lexer->tokenize("EXPECT KEYBOARD UNLOCKED");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 3);
    QCOMPARE(result[0][0].type, TokenType::EXPECT);
    QCOMPARE(result[0][1].type, TokenType::KEYBOARD);
    QCOMPARE(result[0][2].type, TokenType::UNLOCKED);
}

void TestScriptLexer::testWait() {
    auto result = m_lexer->tokenize("WAIT 2000");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0].type, TokenType::WAIT);
    QCOMPARE(result[0][1].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][1].value, "2000");
}

void TestScriptLexer::testGlobalExpectTimeout() {
    auto result = m_lexer->tokenize("GLOBAL EXPECT_TIMEOUT 10000");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 3);
    QCOMPARE(result[0][0].type, TokenType::GLOBAL);
    QCOMPARE(result[0][1].type, TokenType::EXPECT_TIMEOUT);
    QCOMPARE(result[0][2].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][2].value, "10000");
}

void TestScriptLexer::testGlobalDelay() {
    auto result = m_lexer->tokenize("GLOBAL DELAY 100");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 3);
    QCOMPARE(result[0][0].type, TokenType::GLOBAL);
    QCOMPARE(result[0][1].type, TokenType::DELAY);
    QCOMPARE(result[0][2].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][2].value, "100");
}

void TestScriptLexer::testGlobalJitter() {
    auto result = m_lexer->tokenize("GLOBAL JITTER 50 200");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 4);
    QCOMPARE(result[0][0].type, TokenType::GLOBAL);
    QCOMPARE(result[0][1].type, TokenType::JITTER);
    QCOMPARE(result[0][2].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][2].value, "50");
    QCOMPARE(result[0][3].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][3].value, "200");
}

void TestScriptLexer::testVariables() {
    auto result = m_lexer->tokenize("SET $USERNAME \"QSECOFR\"");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 3);
    QCOMPARE(result[0][0].type, TokenType::SET);
    QCOMPARE(result[0][1].type, TokenType::VARIABLE);
    QCOMPARE(result[0][1].value, "$USERNAME");
    QCOMPARE(result[0][2].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][2].value, "QSECOFR");
}

void TestScriptLexer::testSetCommand() {
    auto result = m_lexer->tokenize("INC $COUNTER\nDEC $COUNTER\nADD $COUNTER 5");
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0][0].type, TokenType::INC);
    QCOMPARE(result[0][1].type, TokenType::VARIABLE);
    QCOMPARE(result[1][0].type, TokenType::DEC);
    QCOMPARE(result[2][0].type, TokenType::ADD);
    QCOMPARE(result[2][2].value, "5");
}

void TestScriptLexer::testIfCondition() {
    auto result = m_lexer->tokenize("IF $EXPECT_RESULT == \"TIMEOUT\"");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::IF);
    QCOMPARE(result[0][1].type, TokenType::VARIABLE);
    QCOMPARE(result[0][1].value, "$EXPECT_RESULT");
    QCOMPARE(result[0][2].type, TokenType::OP_EQ);
    QCOMPARE(result[0][3].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][3].value, "TIMEOUT");
}

void TestScriptLexer::testStringLiteralEscapes() {
    auto result = m_lexer->tokenize("SET $X \"hello\\\"world\"");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][2].value, "hello\"world");
}

void TestScriptLexer::testNumberLiterals() {
    auto result = m_lexer->tokenize("WAIT 500");
    QCOMPARE(result[0][1].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][1].value, "500");
}

void TestScriptLexer::testComparisonOperators() {
    auto tests = QStringList{"==", "!=", "<", ">", "<=", ">="};
    auto expected = QVector<TokenType>{
        TokenType::OP_EQ, TokenType::OP_NE, TokenType::OP_LT,
        TokenType::OP_GT, TokenType::OP_LE, TokenType::OP_GE
    };

    for (int i = 0; i < tests.size(); ++i) {
        auto result = m_lexer->tokenize(QString("IF $X %1 5").arg(tests[i]));
        QCOMPARE(result[0][2].type, expected[i]);
    }
}

void TestScriptLexer::testMultilineScript() {
    QString script = "# Test script\nTYPE \"Hello\"\nENTER\nWAIT 500\n";
    auto result = m_lexer->tokenize(script);
    QCOMPARE(result.size(), 3);
    QCOMPARE(result[0][0].type, TokenType::TYPE);
    QCOMPARE(result[1][0].type, TokenType::ENTER);
    QCOMPARE(result[2][0].type, TokenType::WAIT);
}

void TestScriptLexer::testLabelGoto() {
    auto result = m_lexer->tokenize("LABEL retry\nGOTO retry");
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0][0].type, TokenType::LABEL);
    QCOMPARE(result[0][1].value, "retry");
    QCOMPARE(result[1][0].type, TokenType::GOTO);
    QCOMPARE(result[1][1].value, "retry");
}

void TestScriptLexer::testOnTimeoutGoto() {
    auto result = m_lexer->tokenize("ON TIMEOUT GOTO handler");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::ON);
    QCOMPARE(result[0][1].type, TokenType::TIMEOUT);
    QCOMPARE(result[0][2].type, TokenType::GOTO);
    QCOMPARE(result[0][3].value, "handler");
}

void TestScriptLexer::testLogCommand() {
    auto result = m_lexer->tokenize("LOG \"Currently: $TITLE\"");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::LOG);
    QCOMPARE(result[0][1].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][1].value, "Currently: $TITLE");
}

void TestScriptLexer::testAbortCommand() {
    auto result = m_lexer->tokenize("ABORT \"Login failed\"");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::ABORT);
    QCOMPARE(result[0][1].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][1].value, "Login failed");

    // ABORT without message
    result = m_lexer->tokenize("ABORT");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].size(), 1);
    QCOMPARE(result[0][0].type, TokenType::ABORT);
}

void TestScriptLexer::testRepeat() {
    auto result = m_lexer->tokenize("REPEAT 3");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::REPEAT);
    QCOMPARE(result[0][1].type, TokenType::NUMBER_LITERAL);
    QCOMPARE(result[0][1].value, "3");
}

void TestScriptLexer::testDefCallReturn() {
    // DEF with parameters
    auto result = m_lexer->tokenize("DEF login($user, $pass)");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::DEF);
    QCOMPARE(result[0][1].type, TokenType::STRING_LITERAL); // function name as identifier
    QCOMPARE(result[0][1].value, "login");
    QCOMPARE(result[0][2].type, TokenType::LPAREN);
    QCOMPARE(result[0][3].type, TokenType::VARIABLE);
    QCOMPARE(result[0][3].value, "$user");
    QCOMPARE(result[0][4].type, TokenType::COMMA);
    QCOMPARE(result[0][5].type, TokenType::VARIABLE);
    QCOMPARE(result[0][5].value, "$pass");
    QCOMPARE(result[0][6].type, TokenType::RPAREN);

    // ENDDEF
    result = m_lexer->tokenize("ENDDEF");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::ENDDEF);

    // CALL with arguments
    result = m_lexer->tokenize("CALL login(\"admin\", \"secret123\")");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::CALL);
    QCOMPARE(result[0][1].value, "login");
    QCOMPARE(result[0][2].type, TokenType::LPAREN);
    QCOMPARE(result[0][3].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][3].value, "admin");
    QCOMPARE(result[0][4].type, TokenType::COMMA);
    QCOMPARE(result[0][5].type, TokenType::STRING_LITERAL);
    QCOMPARE(result[0][5].value, "secret123");
    QCOMPARE(result[0][6].type, TokenType::RPAREN);

    // CALL with variable arguments
    result = m_lexer->tokenize("CALL login($user, $pass)");
    QCOMPARE(result[0][3].type, TokenType::VARIABLE);
    QCOMPARE(result[0][3].value, "$user");
    QCOMPARE(result[0][5].type, TokenType::VARIABLE);
    QCOMPARE(result[0][5].value, "$pass");

    // RETURN
    result = m_lexer->tokenize("RETURN");
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0][0].type, TokenType::RETURN);

    // DEF with zero args
    result = m_lexer->tokenize("DEF doSomething()");
    QCOMPARE(result[0][0].type, TokenType::DEF);
    QCOMPARE(result[0][1].value, "doSomething");
    QCOMPARE(result[0][2].type, TokenType::LPAREN);
    QCOMPARE(result[0][3].type, TokenType::RPAREN);
}

QTEST_MAIN(TestScriptLexer)
#include "test_script_lexer.moc"
