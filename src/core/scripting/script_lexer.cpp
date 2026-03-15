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

#include "script_lexer.h"

namespace core::scripting {

ScriptLexer::ScriptLexer() {
    initKeywords();
}

void ScriptLexer::initKeywords() {
    // Input — text
    m_keywords["TYPE"]     = TokenType::TYPE;
    m_keywords["PRESS"]    = TokenType::PRESS;

    // AID keys
    m_keywords["ENTER"]    = TokenType::ENTER;
    m_keywords["F1"]       = TokenType::F1;
    m_keywords["F2"]       = TokenType::F2;
    m_keywords["F3"]       = TokenType::F3;
    m_keywords["F4"]       = TokenType::F4;
    m_keywords["F5"]       = TokenType::F5;
    m_keywords["F6"]       = TokenType::F6;
    m_keywords["F7"]       = TokenType::F7;
    m_keywords["F8"]       = TokenType::F8;
    m_keywords["F9"]       = TokenType::F9;
    m_keywords["F10"]      = TokenType::F10;
    m_keywords["F11"]      = TokenType::F11;
    m_keywords["F12"]      = TokenType::F12;
    m_keywords["F13"]      = TokenType::F13;
    m_keywords["F14"]      = TokenType::F14;
    m_keywords["F15"]      = TokenType::F15;
    m_keywords["F16"]      = TokenType::F16;
    m_keywords["F17"]      = TokenType::F17;
    m_keywords["F18"]      = TokenType::F18;
    m_keywords["F19"]      = TokenType::F19;
    m_keywords["F20"]      = TokenType::F20;
    m_keywords["F21"]      = TokenType::F21;
    m_keywords["F22"]      = TokenType::F22;
    m_keywords["F23"]      = TokenType::F23;
    m_keywords["F24"]      = TokenType::F24;
    m_keywords["PAGEUP"]   = TokenType::PAGEUP;
    m_keywords["PAGEDOWN"] = TokenType::PAGEDOWN;
    m_keywords["ATTN"]     = TokenType::ATTN;
    m_keywords["SYSREQ"]   = TokenType::SYSREQ;
    m_keywords["HELP"]     = TokenType::HELP;
    m_keywords["CLEAR"]    = TokenType::CLEAR;
    m_keywords["PRINT"]    = TokenType::PRINT;

    // Local keys
    m_keywords["TAB"]        = TokenType::TAB;
    m_keywords["BACKTAB"]    = TokenType::BACKTAB;
    m_keywords["BACKSPACE"]  = TokenType::BACKSPACE;
    m_keywords["DELETE"]     = TokenType::DELETE_KEY;
    m_keywords["INSERT"]     = TokenType::INSERT;
    m_keywords["HOME"]       = TokenType::HOME;
    m_keywords["END"]        = TokenType::END;
    m_keywords["ESC"]        = TokenType::ESC;
    m_keywords["FIELDPLUS"]  = TokenType::FIELDPLUS;
    m_keywords["FIELDMINUS"] = TokenType::FIELDMINUS;
    m_keywords["FIELDEXIT"]  = TokenType::FIELDEXIT;
    m_keywords["DUP"]        = TokenType::DUP;
    m_keywords["ERASEINPUT"] = TokenType::ERASEINPUT;
    m_keywords["ERASEFIELD"] = TokenType::ERASEFIELD;
    m_keywords["ERASEEOF"]   = TokenType::ERASEEOF;

    // Cursor movement
    m_keywords["MOVE"]     = TokenType::MOVE;

    // Screen inspection
    m_keywords["EXPECT"]   = TokenType::EXPECT;
    m_keywords["EXTRACT"]  = TokenType::EXTRACT;

    // Timing
    m_keywords["WAIT"]     = TokenType::WAIT;

    // Global settings
    m_keywords["GLOBAL"]   = TokenType::GLOBAL;

    // Variables
    m_keywords["SET"]      = TokenType::SET;
    m_keywords["INC"]      = TokenType::INC;
    m_keywords["DEC"]      = TokenType::DEC;
    m_keywords["ADD"]      = TokenType::ADD;

    // Control flow
    m_keywords["IF"]         = TokenType::IF;
    m_keywords["ELSE"]       = TokenType::ELSE;
    m_keywords["ENDIF"]      = TokenType::ENDIF;
    m_keywords["WHILE"]      = TokenType::WHILE;
    m_keywords["ENDWHILE"]   = TokenType::ENDWHILE;
    m_keywords["REPEAT"]     = TokenType::REPEAT;
    m_keywords["ENDREPEAT"]  = TokenType::ENDREPEAT;
    m_keywords["LABEL"]      = TokenType::LABEL;
    m_keywords["GOTO"]       = TokenType::GOTO;

    // Error handling
    m_keywords["ON"]       = TokenType::ON;
    m_keywords["ABORT"]    = TokenType::ABORT;

    // Utility
    m_keywords["LOG"]      = TokenType::LOG;
    m_keywords["PAUSE"]    = TokenType::PAUSE;

    // Sub-tokens
    m_keywords["TEXT"]            = TokenType::TEXT;
    m_keywords["AT"]              = TokenType::AT;
    m_keywords["ROW"]             = TokenType::ROW;
    m_keywords["CURSOR"]          = TokenType::CURSOR;
    m_keywords["KEYBOARD"]        = TokenType::KEYBOARD;
    m_keywords["UNLOCKED"]        = TokenType::UNLOCKED;
    m_keywords["ERRORLOCKED"]     = TokenType::ERRORLOCKED;
    m_keywords["FIELD"]           = TokenType::FIELD;
    m_keywords["CONTAINS"]        = TokenType::CONTAINS;
    m_keywords["MESSAGEWAITING"]  = TokenType::MESSAGEWAITING;
    m_keywords["NOT"]             = TokenType::NOT;
    m_keywords["LENGTH"]          = TokenType::LENGTH;
    m_keywords["FROM"]            = TokenType::FROM;
    m_keywords["COL"]             = TokenType::COL;
    m_keywords["ERROR"]           = TokenType::ERROR;
    m_keywords["TIMEOUT"]         = TokenType::TIMEOUT;
    m_keywords["INPUTFIELD"]      = TokenType::INPUTFIELD;
    m_keywords["NEXT"]            = TokenType::NEXT;
    m_keywords["PREVIOUS"]        = TokenType::PREVIOUS;
    m_keywords["UP"]              = TokenType::UP;
    m_keywords["DOWN"]            = TokenType::DOWN;
    m_keywords["LEFT"]            = TokenType::LEFT;
    m_keywords["RIGHT"]           = TokenType::RIGHT;
    m_keywords["DELAY"]           = TokenType::DELAY;
    m_keywords["JITTER"]          = TokenType::JITTER;
    m_keywords["EXPECT_TIMEOUT"]  = TokenType::EXPECT_TIMEOUT;
}

QVector<TokenLine> ScriptLexer::tokenize(const QString &source) {
    QVector<TokenLine> result;
    const QStringList lines = source.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        TokenLine tl = tokenizeLine(lines[i], i + 1);
        if (!tl.isEmpty())
            result.append(tl);
    }
    return result;
}

TokenLine ScriptLexer::tokenizeLine(const QString &line, int lineNumber) {
    TokenLine tokens;
    int pos = 0;

    skipWhitespace(line, pos);

    // Empty line
    if (pos >= line.length())
        return tokens;

    // Comment line starting with #
    if (line[pos] == '#')
        return tokens;

    // Read the first token to determine line type
    ScriptToken first = nextToken(line, pos, lineNumber);
    if (first.type == TokenType::EOF_TOKEN)
        return tokens;

    tokens.append(first);

    // TYPE: requires a double-quoted string
    if (first.type == TokenType::TYPE) {
        skipWhitespace(line, pos);
        if (pos < line.length() && line[pos] == '"') {
            tokens.append(readStringLiteral(line, pos, lineNumber));
        }
        return tokens;
    }

    // LOG: rest of line is the message (but may be quoted)
    if (first.type == TokenType::LOG) {
        skipWhitespace(line, pos);
        if (pos < line.length()) {
            if (line[pos] == '"') {
                tokens.append(readStringLiteral(line, pos, lineNumber));
            } else {
                QString payload = line.mid(pos);
                tokens.append(ScriptToken(TokenType::STRING_LITERAL, payload, lineNumber, pos + 1));
            }
        }
        return tokens;
    }

    // ABORT: optional message
    if (first.type == TokenType::ABORT) {
        skipWhitespace(line, pos);
        if (pos < line.length() && line[pos] == '"') {
            tokens.append(readStringLiteral(line, pos, lineNumber));
        }
        return tokens;
    }

    // General tokenization for remaining tokens on the line
    while (pos < line.length()) {
        skipWhitespace(line, pos);
        if (pos >= line.length()) break;

        // Inline comment
        if (line[pos] == '#') break;

        ScriptToken tok = nextToken(line, pos, lineNumber);
        if (tok.type == TokenType::EOF_TOKEN) break;
        tokens.append(tok);
    }

    return tokens;
}

ScriptToken ScriptLexer::nextToken(const QString &line, int &pos, int lineNumber) {
    skipWhitespace(line, pos);
    if (pos >= line.length())
        return ScriptToken(TokenType::EOF_TOKEN, "", lineNumber, pos + 1);

    QChar ch = line[pos];

    // String literal
    if (ch == '"')
        return readStringLiteral(line, pos, lineNumber);

    // Variable
    if (ch == '$')
        return readVariable(line, pos, lineNumber);

    // Number (or negative number)
    if (ch.isDigit() || (ch == '-' && pos + 1 < line.length() && line[pos + 1].isDigit()))
        return readNumber(line, pos, lineNumber);

    // Comparison operators
    if (ch == '=' && pos + 1 < line.length() && line[pos + 1] == '=') {
        int col = pos + 1;
        pos += 2;
        return ScriptToken(TokenType::OP_EQ, "==", lineNumber, col);
    }
    if (ch == '!' && pos + 1 < line.length() && line[pos + 1] == '=') {
        int col = pos + 1;
        pos += 2;
        return ScriptToken(TokenType::OP_NE, "!=", lineNumber, col);
    }
    if (ch == '<') {
        int col = pos + 1;
        if (pos + 1 < line.length() && line[pos + 1] == '=') {
            pos += 2;
            return ScriptToken(TokenType::OP_LE, "<=", lineNumber, col);
        }
        pos += 1;
        return ScriptToken(TokenType::OP_LT, "<", lineNumber, col);
    }
    if (ch == '>') {
        int col = pos + 1;
        if (pos + 1 < line.length() && line[pos + 1] == '=') {
            pos += 2;
            return ScriptToken(TokenType::OP_GE, ">=", lineNumber, col);
        }
        pos += 1;
        return ScriptToken(TokenType::OP_GT, ">", lineNumber, col);
    }

    // Punctuation
    if (ch == '(') {
        int col = pos + 1;
        pos++;
        return ScriptToken(TokenType::LPAREN, "(", lineNumber, col);
    }
    if (ch == ')') {
        int col = pos + 1;
        pos++;
        return ScriptToken(TokenType::RPAREN, ")", lineNumber, col);
    }
    if (ch == ',') {
        int col = pos + 1;
        pos++;
        return ScriptToken(TokenType::COMMA, ",", lineNumber, col);
    }

    // Word (keyword or identifier)
    if (ch.isLetter() || ch == '_')
        return readWord(line, pos, lineNumber);

    // Unknown character — skip it
    pos++;
    return ScriptToken(TokenType::EOF_TOKEN, QString(ch), lineNumber, pos);
}

ScriptToken ScriptLexer::readStringLiteral(const QString &line, int &pos, int lineNumber) {
    int col = pos + 1;
    pos++; // skip opening quote
    QString value;
    while (pos < line.length() && line[pos] != '"') {
        if (line[pos] == '\\' && pos + 1 < line.length()) {
            QChar next = line[pos + 1];
            if (next == '"') { value += '"'; pos += 2; continue; }
            if (next == 'n') { value += '\n'; pos += 2; continue; }
            if (next == '\\') { value += '\\'; pos += 2; continue; }
        }
        value += line[pos];
        pos++;
    }
    if (pos < line.length()) pos++; // skip closing quote
    return ScriptToken(TokenType::STRING_LITERAL, value, lineNumber, col);
}

ScriptToken ScriptLexer::readNumber(const QString &line, int &pos, int lineNumber) {
    int col = pos + 1;
    int start = pos;
    if (line[pos] == '-') pos++;
    while (pos < line.length() && line[pos].isDigit()) pos++;
    return ScriptToken(TokenType::NUMBER_LITERAL, line.mid(start, pos - start), lineNumber, col);
}

ScriptToken ScriptLexer::readVariable(const QString &line, int &pos, int lineNumber) {
    int col = pos + 1;
    int start = pos;
    pos++; // skip $
    // Check for $$ (escaped dollar sign) — not a variable
    if (pos < line.length() && line[pos] == '$') {
        pos++;
        return ScriptToken(TokenType::STRING_LITERAL, "$", lineNumber, col);
    }
    while (pos < line.length() && (line[pos].isLetterOrNumber() || line[pos] == '_')) pos++;
    return ScriptToken(TokenType::VARIABLE, line.mid(start, pos - start), lineNumber, col);
}

ScriptToken ScriptLexer::readWord(const QString &line, int &pos, int lineNumber) {
    int col = pos + 1;
    int start = pos;
    while (pos < line.length() && (line[pos].isLetterOrNumber() || line[pos] == '_')) pos++;
    QString word = line.mid(start, pos - start);
    QString upper = word.toUpper();

    auto it = m_keywords.find(upper);
    if (it != m_keywords.end())
        return ScriptToken(it.value(), upper, lineNumber, col);

    // KEY_* character constants
    if (upper.startsWith("KEY_") && upper.length() > 4) {
        QString suffix = upper.mid(4);
        QString ch = resolveKeyChar(suffix);
        if (!ch.isEmpty())
            return ScriptToken(TokenType::KEY_CHAR, ch, lineNumber, col);
    }

    // Not a keyword — treat as string literal (e.g., label name, identifier)
    return ScriptToken(TokenType::STRING_LITERAL, word, lineNumber, col);
}

QString ScriptLexer::resolveKeyChar(const QString &suffix) const {
    // Single letter: KEY_A → "A", KEY_Z → "Z"
    if (suffix.length() == 1 && suffix[0].isLetterOrNumber())
        return suffix;

    // Named special characters
    static const QHash<QString, QString> namedKeys = {
        {"SPACE",         " "},
        {"PERIOD",        "."},
        {"COMMA",         ","},
        {"SEMICOLON",     ";"},
        {"COLON",         ":"},
        {"SLASH",         "/"},
        {"BACKSLASH",     "\\"},
        {"MINUS",         "-"},
        {"PLUS",          "+"},
        {"EQUAL",         "="},
        {"UNDERSCORE",    "_"},
        {"QUOTE",         "'"},
        {"DOUBLEQUOTE",   "\""},
        {"EXCLAIM",       "!"},
        {"QUESTION",      "?"},
        {"AT",            "@"},
        {"HASH",          "#"},
        {"DOLLAR",        "$"},
        {"PERCENT",       "%"},
        {"CARET",         "^"},
        {"AMPERSAND",     "&"},
        {"ASTERISK",      "*"},
        {"PIPE",          "|"},
        {"TILDE",         "~"},
        {"BACKQUOTE",     "`"},
        {"PAREN_LEFT",    "("},
        {"PAREN_RIGHT",   ")"},
        {"BRACKET_LEFT",  "["},
        {"BRACKET_RIGHT", "]"},
        {"BRACE_LEFT",    "{"},
        {"BRACE_RIGHT",   "}"},
        {"LESS",          "<"},
        {"GREATER",       ">"},
    };

    auto it = namedKeys.find(suffix);
    if (it != namedKeys.end())
        return it.value();

    return {};
}

void ScriptLexer::skipWhitespace(const QString &line, int &pos) {
    while (pos < line.length() && line[pos].isSpace()) pos++;
}

} // namespace core::scripting
