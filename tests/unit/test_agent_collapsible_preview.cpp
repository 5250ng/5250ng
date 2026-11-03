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

#include "ui/widgets/QAgentPanelWidget/collapsible_preview.h"
#include <QTextDocument>
#include <QtTest/QtTest>

using ui::widgets::collapsedBlockPreviewHtml;

// The folded preview of a generated script must show the script text, not
// its HTML escapes, on every line, including the dimmed ones.
class TestAgentCollapsiblePreview : public QObject {
    Q_OBJECT

  private slots:
    void dimmedLinesAreNotEscapedTwice() {
        const QString script = QStringLiteral(
            "# @script.name = \"Login\"\n"
            "EXPECT KEYBOARD UNLOCKED\n"
            "TYPE \"QSECOFR\"\n"
            "TYPE \"a<b & c>d\"\n"
            "PRESS ENTER\n"
            "WAIT 1\n");
        const QString fullHtml =
            script.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));

        const QString html =
            collapsedBlockPreviewHtml(fullHtml, "#1a1a2e", "#d4d4d4", "#50c878", 7);

        QTextDocument doc;
        doc.setHtml(html);
        const QString shown = doc.toPlainText();
        QVERIFY2(shown.contains("TYPE \"QSECOFR\""), qPrintable(shown));
        QVERIFY2(shown.contains("TYPE \"a<b & c>d\""), qPrintable(shown)); // dimmed line 1
        QVERIFY2(shown.contains("PRESS ENTER"), qPrintable(shown));        // dimmed line 2
        QVERIFY2(!shown.contains("&quot;") && !shown.contains("&amp;"), qPrintable(shown));
        QVERIFY2(shown.contains("... 2 more lines"), qPrintable(shown));
    }

    void shortBlockIsShownWhole() {
        const QString html =
            collapsedBlockPreviewHtml("TYPE &quot;x&quot;", "#000000", "#ffffff", "#00ff00", 1);
        QTextDocument doc;
        doc.setHtml(html);
        QCOMPARE(doc.toPlainText(), QString("TYPE \"x\""));
    }
};

QTEST_MAIN(TestAgentCollapsiblePreview)
#include "test_agent_collapsible_preview.moc"
