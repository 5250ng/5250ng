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

#include "screen_history.h"

namespace core {

ScreenHistory::ScreenHistory(int maxDepth) : m_maxDepth(maxDepth) {
    m_history.reserve(maxDepth);
}

void ScreenHistory::setMaxDepth(int depth) {
    m_maxDepth = depth;
    while (m_history.size() > m_maxDepth)
        m_history.removeLast();
}

void ScreenHistory::push(const ScreenSnapshot &snapshot) {
    m_history.prepend(snapshot);
    if (m_history.size() > m_maxDepth)
        m_history.removeLast();
}

void ScreenHistory::clear() {
    m_history.clear();
}

const ScreenSnapshot &ScreenHistory::at(int index) const {
    return m_history.at(index);
}

QVector<int> ScreenHistory::search(const QString &text) const {
    QVector<int> results;
    for (int i = 0; i < m_history.size(); ++i) {
        QString screenText = QString::fromUtf8(m_history[i].cellData);
        if (screenText.contains(text, Qt::CaseInsensitive))
            results.append(i);
    }
    return results;
}

} // namespace core
