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

#include "ui/themes/terminal_theme.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using ui::themes::TerminalTheme;
using ui::themes::TerminalThemeManager;

// A theme file is untrusted input: its id must not pick where the embedded
// image is written, and a path it names must not be embedded on re-export.
class TestThemeImportPaths : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_tmp.isValid());
        m_imagesDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + "/terminal_themes/images";
    }

    void traversalIdCannotWriteOutsideImagesDir() {
        // From <AppData>/terminal_themes/images, climb to the test's temporary
        // directory.
        const QString target = m_tmp.filePath("pwn");
        const QString relative = QDir(m_imagesDir).relativeFilePath(target);
        QVERIFY(relative.startsWith(".."));

        const QString id = importFile(themeJson(relative, pngData(), QString()));
        QVERIFY(!id.isEmpty());
        QVERIFY2(!QFile::exists(target + ".png"), "image written outside the themes directory");

        const TerminalTheme t = TerminalThemeManager::instance().theme(id);
        QVERIFY(!t.id.contains('/') && !t.id.contains('.'));
        const QFileInfo image(t.backgroundImagePath);
        QVERIFY(image.exists());
        QCOMPARE(image.absolutePath(), QFileInfo(m_imagesDir).absoluteFilePath());
        QVERIFY(TerminalThemeManager::instance().deleteUserTheme(id));
        QFile::remove(image.absoluteFilePath());
    }

    void idWithNothingUsableGetsGeneratedName() {
        const QString id = importFile(themeJson(QString::fromUtf8("\xe4\xb8\xbb\xe9\xa2\x98"),
                                                QByteArray(), QString()));
        QVERIFY(id.startsWith(QStringLiteral("imported_theme")));
        QVERIFY(TerminalThemeManager::instance().deleteUserTheme(id));
    }

    void importedPathIsNotEmbeddedOnExport() {
        // A theme that only names a local file (no embedded data), such as a
        // private key, must not carry that file out on export.
        const QString secret = m_tmp.filePath("id_rsa");
        {
            QFile f(secret);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("-----BEGIN OPENSSH PRIVATE KEY-----\nsecret\n");
        }
        const QString id = importFile(themeJson("leaky", QByteArray(), secret));
        QVERIFY(!id.isEmpty());

        const QString out = m_tmp.filePath("exported.5250theme");
        QVERIFY(TerminalThemeManager::instance().exportTheme(id, out));
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray exported = f.readAll();
        QVERIFY2(!exported.contains(QByteArray("secret").toBase64().left(6)),
                 "private file embedded in the exported theme");
        const QJsonObject bg = QJsonDocument::fromJson(exported).object()
                                   .value("background").toObject();
        QVERIFY(!bg.contains("imageData"));
        QVERIFY(TerminalThemeManager::instance().deleteUserTheme(id));
    }

    // A theme the user points at a non-image file is not embedded either.
    void exportEmbedsOnlyImages() {
        const QString notImage = m_tmp.filePath("notes.txt");
        {
            QFile f(notImage);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("not an image");
        }
        TerminalTheme t = TerminalThemeManager::instance().resolvedTheme(
            TerminalThemeManager::defaultThemeId());
        t.id = QStringLiteral("export_only_images");
        t.builtin = false;
        t.backgroundMode = TerminalTheme::Image;
        t.backgroundImagePath = notImage;
        QVERIFY(TerminalThemeManager::instance().saveUserTheme(t));
        const QString out = m_tmp.filePath("only-images.5250theme");
        QVERIFY(TerminalThemeManager::instance().exportTheme(t.id, out));
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject bg = QJsonDocument::fromJson(f.readAll()).object()
                                   .value("background").toObject();
        QVERIFY(!bg.contains("imageData"));
        QVERIFY(TerminalThemeManager::instance().deleteUserTheme(t.id));
    }

  private:
    static QByteArray pngData() {
        QImage img(2, 2, QImage::Format_RGB32);
        img.fill(Qt::red);
        QByteArray data;
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        return data;
    }

    static QByteArray themeJson(const QString &id, const QByteArray &image,
                                const QString &imagePath) {
        QJsonObject bg{{"mode", "image"}, {"color", "#000000"}};
        if (!image.isEmpty())
            bg["imageData"] = QString::fromLatin1(image.toBase64());
        if (!imagePath.isEmpty())
            bg["imagePath"] = imagePath;
        QJsonObject theme{{"id", id}, {"displayName", "Imported"}, {"background", bg}};
        return QJsonDocument(theme).toJson();
    }

    // Imports the JSON and returns the id the manager registered for it.
    QString importFile(const QByteArray &json) {
        const QString path = m_tmp.filePath("in.5250theme");
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return QString();
        f.write(json);
        f.close();
        const QStringList before = TerminalThemeManager::instance().availableThemes();
        if (!TerminalThemeManager::instance().importTheme(path))
            return QString();
        for (const QString &id : TerminalThemeManager::instance().availableThemes())
            if (!before.contains(id))
                return id;
        return QString();
    }

    QTemporaryDir m_tmp;
    QString m_imagesDir;
};

QTEST_MAIN(TestThemeImportPaths)
#include "test_theme_import_paths.moc"
