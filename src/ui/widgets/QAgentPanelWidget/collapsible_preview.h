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
#include <QString>
#include <QStringList>

namespace ui::widgets {

/// Folded view of a collapsible agent-panel block: the first three lines of
/// fullHtml, two progressively dimmed lines and a "... N more lines" link.
/// fullHtml is already HTML (escaped text joined with <br/>), so its lines
/// are inserted as they are.
inline QString collapsedBlockPreviewHtml(const QString &fullHtml, const QString &codeBg,
                                         const QString &codeText, const QString &linkColor,
                                         int blockId) {
    QStringList lines = fullHtml.split(QStringLiteral("<br/>"));
    if (lines.size() <= 1)
        lines = fullHtml.split(QStringLiteral("<br>"));

    int totalLines = lines.size();
    int visibleLines = qMin(3, totalLines);
    QString content = lines.mid(0, visibleLines).join(QStringLiteral("<br/>"));

    if (totalLines > visibleLines) {
        // Add 1-2 progressively faded lines
        int fadeCount = qMin(2, totalLines - visibleLines);
        // Opacity steps: 50%, 25%
        const int opacities[] = {128, 64};
        for (int i = 0; i < fadeCount; ++i) {
            // QTextBrowser doesn't support rgba(), so blend codeText toward
            // codeBg by hand
            QColor bg(codeBg), fg(codeText);
            double a = opacities[i] / 255.0;
            QColor blended(
                int(fg.red() * a + bg.red() * (1 - a)),
                int(fg.green() * a + bg.green() * (1 - a)),
                int(fg.blue() * a + bg.blue() * (1 - a)));

            content += QStringLiteral("<br/><span style='color:%1;'>%2</span>")
                .arg(blended.name(), lines[visibleLines + i]);
        }

        int remaining = totalLines - visibleLines - fadeCount;
        if (remaining > 0) {
            content += QStringLiteral(
                "<br/><a href='toggle://%1' style='color:%2; font-size:11px;'>"
                "... %3 more lines</a>")
                .arg(blockId).arg(linkColor).arg(remaining);
        }
    }
    return content;
}

} // namespace ui::widgets
