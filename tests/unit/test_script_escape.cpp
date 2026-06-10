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

#include "mcp/script_escape.h"

#include <5250script/script_lexer.h>
#include <QtTest/QtTest>

using mcp::escapeScriptString;
using core::scripting::ScriptLexer;
using core::scripting::ScriptToken;
using core::scripting::TokenType;

// Regression tests for issue #155... (MCP script-injection escaping, #N).
// The MCP tool handlers embed user-supplied strings into 5250script
// double-quoted literals as TYPE "<escaped>". escapeScriptString must
// produce a literal that the real ScriptLexer decodes back to exactly the
// original string, as a single token, with no injected statements —
// regardless of quotes, backslashes, or newlines in the input.
class TestScriptEscape : public QObject {
    Q_OBJECT

  private slots:
    void roundTrip_data();
    void roundTrip();
    void trailingBackslashDoesNotUnterminate();
    void newlineDoesNotInjectStatements();

  private:
    // Lex `TYPE "<escaped(input)>"` and return the decoded string literal,
    // or a null QString if the line did not lex to TYPE + one STRING_LITERAL.
    static QString lexTypeArgument(const QString &input, int *lineCount) {
        ScriptLexer lexer;
        const QString script =
            QStringLiteral("TYPE \"%1\"").arg(escapeScriptString(input));
        auto lines = lexer.tokenize(script);
        if (lineCount) *lineCount = static_cast<int>(lines.size());
        if (lines.size() != 1) return QString();
        const auto &toks = lines.front();
        // Expect: TYPE keyword/identifier, then a single STRING_LITERAL.
        const ScriptToken *strTok = nullptr;
        int stringCount = 0;
        for (const auto &t : toks) {
            if (t.type == TokenType::STRING_LITERAL) {
                strTok = &t;
                stringCount++;
            }
            if (t.type == TokenType::UNKNOWN) return QString();
        }
        if (stringCount != 1 || !strTok) return QString();
        return strTok->value;
    }
};

void TestScriptEscape::roundTrip_data() {
    QTest::addColumn<QString>("input");
    QTest::newRow("plain") << QStringLiteral("HELLO");
    QTest::newRow("spaces") << QStringLiteral("user name");
    QTest::newRow("double quote") << QStringLiteral("say \"hi\"");
    QTest::newRow("single backslash") << QStringLiteral("a\\b");
    QTest::newRow("double backslash") << QStringLiteral("a\\\\b");
    QTest::newRow("tab") << QStringLiteral("a\tb");
    QTest::newRow("password with metachars")
        << QStringLiteral("p@ss\"w\\rd");
    QTest::newRow("only backslash") << QStringLiteral("\\");
    QTest::newRow("only quote") << QStringLiteral("\"");
}

void TestScriptEscape::roundTrip() {
    QFETCH(QString, input);
    int lineCount = 0;
    QString decoded = lexTypeArgument(input, &lineCount);
    QCOMPARE(lineCount, 1);
    QCOMPARE(decoded, input);
}

void TestScriptEscape::trailingBackslashDoesNotUnterminate() {
    // A trailing backslash in the input must not escape the closing quote.
    int lineCount = 0;
    QString decoded = lexTypeArgument(QStringLiteral("PASSWORD\\"), &lineCount);
    QCOMPARE(lineCount, 1);
    QCOMPARE(decoded, QStringLiteral("PASSWORD\\"));
}

void TestScriptEscape::newlineDoesNotInjectStatements() {
    // A newline must be encoded as \n inside the literal, not split the
    // script into multiple physical lines (which would inject statements).
    int lineCount = 0;
    QString decoded = lexTypeArgument(
        QStringLiteral("user\nPRESS ENTER"), &lineCount);
    QCOMPARE(lineCount, 1);
    QCOMPARE(decoded, QStringLiteral("user\nPRESS ENTER"));
}

QTEST_MAIN(TestScriptEscape)
#include "test_script_escape.moc"
