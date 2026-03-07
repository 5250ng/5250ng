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

#include <QChar>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QVector>
#include <cstdint>

namespace core {

struct MatchReplaceRule {
    QString pattern;
    QString replacement;
    bool isRegex = false;
    bool caseSensitive = false;
    bool enabled = true;
};

// Applies match-and-replace rules to the decoded screen text at render time.
// Maintains a parallel overlay buffer of QChars so the original EBCDIC screen
// buffer is never modified — toggling off restores the original display instantly.
class MatchReplaceEngine : public QObject {
    Q_OBJECT

  public:
    explicit MatchReplaceEngine(QObject *parent = nullptr);

    // Master toggle
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on);

    // Rules
    const QVector<MatchReplaceRule> &rules() const { return m_rules; }
    void setRules(const QVector<MatchReplaceRule> &rules);

    // Call after each screen update to rebuild the overlay.
    // decodedLines: one QString per screen row (already EBCDIC→Unicode).
    void rebuildOverlay(const QVector<QString> &decodedLines, int rows, int cols);

    // Query overlay at render time
    bool hasOverlay(int row, int col) const;
    QChar overlayChar(int row, int col) const;

  signals:
    void enabledChanged(bool enabled);
    void rulesChanged();

  private:
    QString applyRules(const QString &line) const;

    bool m_enabled = false;
    QVector<MatchReplaceRule> m_rules;

    // Flat overlay buffer (rows * cols)
    QVector<QChar> m_overlay;
    QVector<bool> m_overlayActive;
    int m_rows = 0;
    int m_cols = 0;
};

} // namespace core
