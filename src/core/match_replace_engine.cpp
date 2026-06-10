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

#include "match_replace_engine.h"
#include "ebcdic.h"

namespace core {

MatchReplaceEngine::MatchReplaceEngine(QObject *parent) : QObject(parent) {}

void MatchReplaceEngine::setEnabled(bool on) {
    if (m_enabled != on) {
        m_enabled = on;
        emit enabledChanged(on);
    }
}

void MatchReplaceEngine::setRules(const QVector<MatchReplaceRule> &rules) {
    m_rules = rules;
    emit rulesChanged();
}

void MatchReplaceEngine::rebuildOverlay(const QVector<QString> &decodedLines, int rows, int cols) {
    m_rows = rows;
    m_cols = cols;
    int total = rows * cols;
    m_overlay.resize(total);
    m_overlayActive.resize(total);
    m_overlayActive.fill(false);

    if (!m_enabled || m_rules.isEmpty()) return;

    for (int r = 0; r < rows && r < decodedLines.size(); ++r) {
        const QString &original = decodedLines[r];
        QString replaced = applyRules(original);
        if (replaced == original) continue;

        int base = r * cols;
        // Cover the full extent of both lines: when the replacement shrinks
        // the row, the cells past the end of `replaced` must be overlaid
        // with blanks, otherwise the original (now shifted-out) characters
        // keep showing there — leaking exactly the text the rule rewrote.
        int len = qMin(qMax(replaced.size(), original.size()), qsizetype(cols));
        for (int c = 0; c < len; ++c) {
            QChar origCh = (c < original.size()) ? original[c] : QChar(' ');
            QChar replCh = (c < replaced.size()) ? replaced[c] : QChar(' ');
            if (replCh != origCh) {
                m_overlay[base + c] = replCh;
                m_overlayActive[base + c] = true;
            }
        }
        // If replacement is longer than cols, truncated (grid is fixed-width)
    }
}

bool MatchReplaceEngine::hasOverlay(int row, int col) const {
    int idx = row * m_cols + col;
    if (idx < 0 || idx >= m_overlayActive.size()) return false;
    return m_overlayActive[idx];
}

QChar MatchReplaceEngine::overlayChar(int row, int col) const {
    int idx = row * m_cols + col;
    if (idx < 0 || idx >= m_overlay.size()) return QChar(' ');
    return m_overlay[idx];
}

QString MatchReplaceEngine::applyRules(const QString &line) const {
    QString result = line;
    for (const auto &rule : m_rules) {
        if (!rule.enabled || rule.pattern.isEmpty()) continue;

        if (rule.isRegex) {
            QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
            if (!rule.caseSensitive)
                opts |= QRegularExpression::CaseInsensitiveOption;
            QRegularExpression re(rule.pattern, opts);
            if (re.isValid()) {
                result.replace(re, rule.replacement);
            }
        } else {
            Qt::CaseSensitivity cs = rule.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
            result.replace(rule.pattern, rule.replacement, cs);
        }
    }
    return result;
}

} // namespace core
