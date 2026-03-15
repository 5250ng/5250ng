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

#include "script_token.h"
#include <QHash>
#include <QString>

namespace core::scripting {

class ScriptLexer {
  public:
    ScriptLexer();

    // Tokenize entire script text, returning one TokenLine per source line
    QVector<TokenLine> tokenize(const QString &source);

    // Tokenize a single line (1-based lineNumber for error reporting)
    TokenLine tokenizeLine(const QString &line, int lineNumber);

  private:
    void initKeywords();
    ScriptToken nextToken(const QString &line, int &pos, int lineNumber);
    ScriptToken readStringLiteral(const QString &line, int &pos, int lineNumber);
    ScriptToken readNumber(const QString &line, int &pos, int lineNumber);
    ScriptToken readVariable(const QString &line, int &pos, int lineNumber);
    ScriptToken readWord(const QString &line, int &pos, int lineNumber);
    QString resolveKeyChar(const QString &suffix) const;
    void skipWhitespace(const QString &line, int &pos);

    QHash<QString, TokenType> m_keywords;
};

} // namespace core::scripting
