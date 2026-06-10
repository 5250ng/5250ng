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

#include "core/match_replace_engine.h"

#include <QtTest/QtTest>

using core::MatchReplaceEngine;
using core::MatchReplaceRule;

class TestMatchReplace : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testSameLengthReplacementOverlaysDiffs();
    void testShorterReplacementBlanksDisplacedTail();
    void testShorterReplacementMasksFullSecret();
    void testLongerReplacementTruncatedAtCols();
    void testDisabledEngineProducesNoOverlay();

  private:
    // Renders row 0 of the overlay applied over `original`, the way the
    // screen widget does at paint time.
    QString renderedRow(const QString &original, int cols) const {
        QString out;
        for (int c = 0; c < cols; ++c) {
            if (m_engine->hasOverlay(0, c)) {
                out += m_engine->overlayChar(0, c);
            } else {
                out += (c < original.size()) ? original[c] : QChar(' ');
            }
        }
        return out;
    }

    MatchReplaceEngine *m_engine = nullptr;
};

void TestMatchReplace::init() {
    m_engine = new MatchReplaceEngine(this);
    m_engine->setEnabled(true);
}

void TestMatchReplace::cleanup() {
    delete m_engine;
    m_engine = nullptr;
}

void TestMatchReplace::testSameLengthReplacementOverlaysDiffs() {
    m_engine->setRules({MatchReplaceRule{"PROD", "TEST", false, true, true}});
    const int cols = 20;
    QString row = QStringLiteral("SYS: PROD READY     ");
    m_engine->rebuildOverlay({row}, 1, cols);
    QCOMPARE(renderedRow(row, cols), QStringLiteral("SYS: TEST READY     "));
}

// Regression test for issue #147: a replacement shorter than its match must
// not leave the displaced original characters visible after the replaced
// text.
void TestMatchReplace::testShorterReplacementBlanksDisplacedTail() {
    m_engine->setRules({MatchReplaceRule{"LONGVALUE", "X", false, true, true}});
    const int cols = 20;
    QString row = QStringLiteral("A LONGVALUE B       ");
    m_engine->rebuildOverlay({row}, 1, cols);
    QCOMPARE(renderedRow(row, cols), QStringLiteral("A X B               "));
}

void TestMatchReplace::testShorterReplacementMasksFullSecret() {
    // The motivating case: masking a value at the end of the row. Before the
    // fix the rendered row ended in "***WORD" because cells past the end of
    // the shrunk line kept their original characters.
    m_engine->setRules({MatchReplaceRule{"PASSWORD", "***", false, true, true}});
    const int cols = 12;
    QString row = QStringLiteral("    PASSWORD");
    m_engine->rebuildOverlay({row}, 1, cols);
    QString rendered = renderedRow(row, cols);
    QCOMPARE(rendered, QStringLiteral("    ***     "));
    QVERIFY(!rendered.contains(QStringLiteral("WORD")));
}

void TestMatchReplace::testLongerReplacementTruncatedAtCols() {
    m_engine->setRules({MatchReplaceRule{"AB", "0123456789", false, true, true}});
    const int cols = 8;
    QString row = QStringLiteral("      AB");
    m_engine->rebuildOverlay({row}, 1, cols);
    QCOMPARE(renderedRow(row, cols), QStringLiteral("      01"));
}

void TestMatchReplace::testDisabledEngineProducesNoOverlay() {
    m_engine->setRules({MatchReplaceRule{"A", "B", false, true, true}});
    m_engine->setEnabled(false);
    const int cols = 4;
    QString row = QStringLiteral("AAAA");
    m_engine->rebuildOverlay({row}, 1, cols);
    QCOMPARE(renderedRow(row, cols), row);
}

QTEST_MAIN(TestMatchReplace)
#include "test_match_replace.moc"
