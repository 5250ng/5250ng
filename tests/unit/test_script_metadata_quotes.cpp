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

#include "core/macro_recorder.h"
#include "core/macro_to_script.h"
#include "core/script_library.h"
#include <5250script/script_compiler.h>
#include <QFile>
#include <QStandardPaths>
#include <QtTest/QtTest>

using core::scripting::ScriptCompiler;

// Names and descriptions written into "# @key = \"value\"" lines must read
// back unchanged, including double quotes and backslashes.
class TestScriptMetadataQuotes : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void macroToScriptKeepsName_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("quotes") << QString("my \"quoted\" macro");
        QTest::newRow("backslash") << QString("C:\\temp\\");
    }

    void macroToScriptKeepsName() {
        QFETCH(QString, name);
        core::Macro macro;
        macro.name = name;
        macro.description = "say \"hi\"";
        const auto meta = ScriptCompiler::extractMetadata(core::macroToScript(macro));
        QCOMPARE(meta.name(), name);
        QCOMPARE(meta.description(), QString("say \"hi\""));
    }

    void libraryTemplateKeepsName() {
        const QString name = "my \"quoted\" script";
        const QString path = core::createScriptFromTemplate(name);
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto meta = ScriptCompiler::extractMetadata(QString::fromUtf8(f.readAll()));
        f.close();
        QFile::remove(path);
        QCOMPARE(meta.name(), name);
        QCOMPARE(meta.menuPath(), name);
    }
};

QTEST_MAIN(TestScriptMetadataQuotes)
#include "test_script_metadata_quotes.moc"
