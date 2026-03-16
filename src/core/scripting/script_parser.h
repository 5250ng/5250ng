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

#pragma once

#include "script_ast.h"
#include "script_lexer.h"

namespace core::scripting {

class ScriptParser {
  public:
    ScriptParser();

    ParseResult parse(const QString &source);
    ParseResult parse(const QVector<TokenLine> &tokenLines);

  private:
    // Parse a single token line into an AST node
    std::shared_ptr<ASTNode> parseLine(const TokenLine &tokens);

    // Specific parsers
    std::shared_ptr<ASTNode> parseExpect(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseExtract(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseSet(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseAdd(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseIf(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseWhile(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseOn(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseMoveCursor(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parsePressKey(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseDef(const TokenLine &tokens);
    std::shared_ptr<ASTNode> parseCall(const TokenLine &tokens);
    bool isAIDKeyToken(TokenType type) const;
    bool isLocalKeyToken(TokenType type) const;

    // Helpers
    void error(int line, const QString &msg);
    bool parseCondition(const TokenLine &tokens, int startIndex,
                        QString &left, CompareOp &op, QString &right);
    uint8_t aidByteForToken(TokenType type) const;

    QVector<ParseError> m_errors;
    ScriptLexer m_lexer;
};

} // namespace core::scripting
