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

#include <QColor>
#include <QRect>
#include <QString>
#include <QVector>
#include <cstdint>

namespace core {

struct Hotspot {
    int row;
    int startCol;
    int endCol;
    QString label;
    uint8_t aidByte;     // AID code to send (e.g., F3 = 0x33)
    QString menuNumber;  // For menu items like "1. User tasks"

    enum Type { FunctionKey, MenuItem };
    Type type;
};

class HotspotDetector {
  public:
    HotspotDetector() = default;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Scan screen buffer text for hotspot patterns
    // screenText: row-major array of Unicode chars (rows x cols)
    QVector<Hotspot> detect(const QVector<QChar> &screenText, int rows, int cols) const;

    // Find hotspot at a given cell position
    static const Hotspot *hotspotAt(const QVector<Hotspot> &hotspots, int row, int col);

  private:
    bool m_enabled = false;

    void detectFunctionKeys(const QString &line, int row, QVector<Hotspot> &out) const;
    void detectMenuItems(const QString &line, int row, QVector<Hotspot> &out) const;

    static uint8_t fkeyToAID(int fkeyNum);
};

} // namespace core
