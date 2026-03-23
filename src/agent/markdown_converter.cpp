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

#include "markdown_converter.h"
#include <QRegularExpression>
#include <QTextDocument>

namespace agent {

QString markdownToHtml(const QString &markdown,
                       const QString &codeBgColor,
                       const QString &codeTextColor) {
    QTextDocument doc;
    doc.setMarkdown(markdown);
    QString html = doc.toHtml();

    // Strip the outer wrapper that QTextDocument::toHtml() produces.
    // We only want the inner body content.
    static const QRegularExpression bodyOpen(
        QStringLiteral("<body[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bodyClose(
        QStringLiteral("</body>"), QRegularExpression::CaseInsensitiveOption);

    int start = -1;
    auto m = bodyOpen.match(html);
    if (m.hasMatch())
        start = m.capturedEnd();
    int end = -1;
    m = bodyClose.match(html);
    if (m.hasMatch())
        end = m.capturedStart();

    if (start >= 0 && end > start)
        html = html.mid(start, end - start).trimmed();

    // Style <pre> blocks (fenced code blocks) with background color.
    // Qt's toHtml() emits <pre> for code blocks.
    QString preStyle = QStringLiteral(
        "style=\"background-color:%1; color:%2; padding:8px; "
        "font-family:'Courier New',monospace;\"")
        .arg(codeBgColor, codeTextColor);
    html.replace(QStringLiteral("<pre "), QStringLiteral("<pre ") + preStyle + ' ');
    // Handle bare <pre> without attributes
    html.replace(QStringLiteral("<pre>"),
                 QStringLiteral("<pre %1>").arg(preStyle));

    // Style inline <code> elements
    QString codeStyle = QStringLiteral(
        "style=\"background-color:%1; color:%2; padding:1px 4px; "
        "font-family:'Courier New',monospace;\"")
        .arg(codeBgColor, codeTextColor);
    html.replace(QStringLiteral("<code>"),
                 QStringLiteral("<code %1>").arg(codeStyle));
    html.replace(QStringLiteral("<code "), QStringLiteral("<code ") + codeStyle + ' ');

    return html;
}

} // namespace agent
