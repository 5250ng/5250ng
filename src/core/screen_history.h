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

#include <QByteArray>
#include <QDateTime>
#include <QVector>
#include <cstdint>

namespace core {

struct ScreenSnapshot {
    QDateTime timestamp;
    QByteArray cellData;   // Serialized ScreenCell array (raw bytes)
    int rows = 0;
    int cols = 0;
    int cursorRow = 0;
    int cursorCol = 0;
};

class ScreenHistory {
  public:
    explicit ScreenHistory(int maxDepth = 100);

    void setMaxDepth(int depth);
    int maxDepth() const { return m_maxDepth; }

    void push(const ScreenSnapshot &snapshot);
    void clear();

    int count() const { return m_history.size(); }
    bool isEmpty() const { return m_history.isEmpty(); }

    // Index 0 = most recent, count()-1 = oldest
    const ScreenSnapshot &at(int index) const;

    // Search screen text content
    QVector<int> search(const QString &text) const;

  private:
    int m_maxDepth;
    QVector<ScreenSnapshot> m_history;
};

} // namespace core
