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

#include "agent/tool_definitions.h"
#include "mcp/McpSessionRegistry.h"
#include "mcp/McpToolHandler.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestMcpWriteFile : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void testMissingContentLeavesFileUntouched_data();
    void testMissingContentLeavesFileUntouched();
    void testEmptyContentTruncates();

  private:
    static QString resultText(const QJsonObject &result);
    static QByteArray readAll(const QString &path);

    QTemporaryDir *m_dir = nullptr;
    mcp::McpSessionRegistry *m_registry = nullptr;
    mcp::McpToolHandler *m_handler = nullptr;
};

static const QByteArray kOriginal = QByteArrayLiteral("original content");

QString TestMcpWriteFile::resultText(const QJsonObject &result) {
    return result.value("content").toArray().first().toObject().value("text").toString();
}

QByteArray TestMcpWriteFile::readAll(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

void TestMcpWriteFile::init() {
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_registry = new mcp::McpSessionRegistry();
    m_handler = new mcp::McpToolHandler(m_registry);
}

void TestMcpWriteFile::cleanup() {
    delete m_handler;
    delete m_registry;
    delete m_dir;
}

// A call without a string "content" must fail and must not truncate an
// existing file.
void TestMcpWriteFile::testMissingContentLeavesFileUntouched_data() {
    QTest::addColumn<QJsonValue>("content");
    QTest::newRow("absent") << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("null") << QJsonValue(QJsonValue::Null);
    QTest::newRow("number") << QJsonValue(42);
}

void TestMcpWriteFile::testMissingContentLeavesFileUntouched() {
    QFETCH(QJsonValue, content);
    const QString path = m_dir->filePath("target.txt");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kOriginal);
    }

    QJsonObject args{{"path", path}};
    if (!content.isUndefined())
        args.insert("content", content);
    const QJsonObject result = m_handler->callTool(agent::kToolWriteFile, args);

    QVERIFY(result.value("isError").toBool());
    QVERIFY2(resultText(result).contains("content"), qPrintable(resultText(result)));
    QCOMPARE(readAll(path), kOriginal);
}

// An explicit empty string is a valid request to empty the file.
void TestMcpWriteFile::testEmptyContentTruncates() {
    const QString path = m_dir->filePath("target.txt");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kOriginal);
    }

    const QJsonObject result = m_handler->callTool(
        agent::kToolWriteFile, QJsonObject{{"path", path}, {"content", QString()}});

    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(readAll(path), QByteArray());
}

QTEST_MAIN(TestMcpWriteFile)
#include "test_mcp_write_file.moc"
