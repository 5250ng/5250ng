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

#include "agent/auth/token_storage.h"
#include "agent/config.h"
#include "core/settings_permissions.h"
#include <QDateTime>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// The settings file holds AI provider API keys and OAuth tokens; it must
// end up readable by its owner only.
class TestSecretSettingsPermissions : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
#ifdef Q_OS_WIN
        QSKIP("Settings are stored in the user's registry hive on Windows");
#endif
        QVERIFY(m_dir.isValid());
        // An INI file in a temporary directory on every platform (macOS's
        // native format is CFPreferences, which setPath() does not redirect)
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_dir.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    }

    void init() {
        // Start from a file anyone can read, as the umask usually leaves it
        QSettings settings;
        settings.setValue("Unrelated/value", 1);
        settings.sync();
        m_path = settings.fileName();
        QVERIFY(QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther));
    }

    void savingApiKeysRestrictsTheFile() {
        auto &config = agent::AgentConfig::instance();
        config.setAnthropicApiKey(QStringLiteral("sk-test-secret"));
        config.save();
        QVERIFY(QSettings().value("AI/anthropicApiKey").toString() == "sk-test-secret");
        checkOwnerOnly();
    }

    void savingTokensRestrictsTheFile() {
        agent::TokenStorage::instance().saveTokens(QStringLiteral("anthropic"), "access", "refresh",
                                                   QDateTime::currentDateTimeUtc());
        checkOwnerOnly();
    }

    // A file written before the fix is fixed when the configuration loads.
    void loadingRestrictsAnExistingFile() {
        agent::AgentConfig::instance().load();
        checkOwnerOnly();
    }

    // On a fresh install the file is created owner-only before any secret
    // is written to it.
    void newFileIsCreatedOwnerOnly() {
        QFile::remove(m_path);
        QSettings settings;
        settings.setValue("AI/anthropicApiKey", "sk-new");
        core::restrictSettingsToOwner(settings);
        checkOwnerOnly();
        QCOMPARE(QSettings().value("AI/anthropicApiKey").toString(), QString("sk-new"));
    }

  private:
    void checkOwnerOnly() {
        const QFileDevice::Permissions p = QFile::permissions(m_path);
        QVERIFY(p.testFlag(QFileDevice::ReadOwner));
        QCOMPARE(p & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther
                      | QFileDevice::WriteOther),
                 QFileDevice::Permissions{});
    }

    QTemporaryDir m_dir;
    QString m_path;
};

QTEST_GUILESS_MAIN(TestSecretSettingsPermissions)
#include "test_secret_settings_permissions.moc"
