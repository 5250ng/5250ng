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

#include "session/config.h"
#include "session/manager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>
#include <QtTest/QtTest>

using namespace session;

class TestSessionConfig : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testInitialization();
    void testSettersGetters();
    void testValidation();
    void testSerialization();
    void testDeserialization();
    void testDeserializationRejectsOutOfRange();
    void testAllowInvalidCertificatesDefaultsOff();
    void testAllowInvalidCertificatesRoundTrip();
    void testPcCommandPolicyDefaults();
    void testPcCommandPolicyRoundTrip();
    void testPcCommandPolicyLegacyConfigCompat();
    void testDistinctNamesDoNotCollideOnDisk();
    void testLegacySessionFilenameLoads();

  private:
    SessionConfig *m_config;
};

void TestSessionConfig::init() { m_config = new SessionConfig(this); }

void TestSessionConfig::cleanup() {
    m_config->deleteLater();
    m_config = nullptr;
}

void TestSessionConfig::testInitialization() {
    QCOMPARE(m_config->name(), QString("New Session"));
    QCOMPARE(m_config->port(), static_cast<quint16>(23));
    QCOMPARE(m_config->useTLS(), false);
    QCOMPARE(m_config->deviceType(), QString("IBM-3179-2"));
    QCOMPARE(m_config->deviceName(), QString(""));
    QCOMPARE(m_config->screenRows(), 24);
    QCOMPARE(m_config->screenCols(), 80);
}

void TestSessionConfig::testSettersGetters() {
    m_config->setName("Test Session");
    QCOMPARE(m_config->name(), QString("Test Session"));

    m_config->setHostname("example.com");
    QCOMPARE(m_config->hostname(), QString("example.com"));

    m_config->setPort(992);
    QCOMPARE(m_config->port(), static_cast<quint16>(992));

    m_config->setUseTLS(true);
    QCOMPARE(m_config->useTLS(), true);

    m_config->setDeviceName("TEST5250");
    QCOMPARE(m_config->deviceName(), QString("TEST5250"));

    m_config->setScreenRows(27);
    QCOMPARE(m_config->screenRows(), 27);

    m_config->setScreenCols(132);
    QCOMPARE(m_config->screenCols(), 132);
}

void TestSessionConfig::testValidation() {
    // Empty config should be invalid
    SessionConfig empty;
    QVERIFY(!empty.isValid());

    // Valid config
    m_config->setName("Test");
    m_config->setHostname("example.com");
    m_config->setPort(23);
    m_config->setScreenRows(24);
    m_config->setScreenCols(80);
    QVERIFY(m_config->isValid());

    // Invalid: empty hostname
    m_config->setHostname("");
    QVERIFY(!m_config->isValid());

    // Invalid: port out of range
    m_config->setHostname("example.com");
    m_config->setPort(0);
    QVERIFY(!m_config->isValid());
}

void TestSessionConfig::testSerialization() {
    m_config->setName("Test Session");
    m_config->setHostname("example.com");
    m_config->setPort(992);
    m_config->setUseTLS(true);
    m_config->setDeviceName("TEST5250");
    m_config->setScreenRows(27);
    m_config->setScreenCols(132);

    QJsonObject json = m_config->toJson();

    QVERIFY(json.contains("name"));
    QVERIFY(json.contains("hostname"));
    QVERIFY(json.contains("port"));
    QVERIFY(json.contains("useTLS"));
    QVERIFY(json.contains("deviceName"));
    QVERIFY(json.contains("screenRows"));
    QVERIFY(json.contains("screenCols"));

    QCOMPARE(json["name"].toString(), QString("Test Session"));
    QCOMPARE(json["hostname"].toString(), QString("example.com"));
    QCOMPARE(json["port"].toInt(), 992);
    QCOMPARE(json["useTLS"].toBool(), true);
}

void TestSessionConfig::testDeserialization() {
    QJsonObject json;
    json["name"] = "Loaded Session";
    json["hostname"] = "test.example.com";
    json["port"] = 992;
    json["useTLS"] = true;
    json["deviceType"] = "IBM-3477-FC";
    json["deviceName"] = "MYTERM01";
    json["screenRows"] = 27;
    json["screenCols"] = 132;

    SessionConfig loaded;
    QVERIFY(loaded.fromJson(json));

    QCOMPARE(loaded.name(), QString("Loaded Session"));
    QCOMPARE(loaded.hostname(), QString("test.example.com"));
    QCOMPARE(loaded.port(), static_cast<quint16>(992));
    QCOMPARE(loaded.useTLS(), true);
    QCOMPARE(loaded.deviceType(), QString("IBM-3477-FC"));
    QCOMPARE(loaded.deviceName(), QString("MYTERM01"));
    QCOMPARE(loaded.screenRows(), 27);
    QCOMPARE(loaded.screenCols(), 132);
}

void TestSessionConfig::testDeserializationRejectsOutOfRange() {
    const auto makeBase = []() {
        QJsonObject base;
        base["name"] = "X";
        base["hostname"] = "h";
        base["port"] = 23;
        base["useTLS"] = false;
        base["deviceType"] = "IBM-3179-2";
        base["deviceName"] = "T";
        base["screenRows"] = 24;
        base["screenCols"] = 80;
        return base;
    };

    // Port out of range
    {
        QJsonObject j = makeBase();
        j["port"] = 70000;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
        QCOMPARE(cfg.port(), static_cast<quint16>(23));
    }
    {
        QJsonObject j = makeBase();
        j["port"] = 0;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
    }
    {
        QJsonObject j = makeBase();
        j["port"] = -1;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
    }
    // Rows out of range
    {
        QJsonObject j = makeBase();
        j["screenRows"] = 0;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
        QCOMPARE(cfg.screenRows(), 24);
    }
    {
        QJsonObject j = makeBase();
        j["screenRows"] = 500;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
    }
    // Cols out of range
    {
        QJsonObject j = makeBase();
        j["screenCols"] = 0;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
    }
    {
        QJsonObject j = makeBase();
        j["screenCols"] = 999;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
    }
    // Unknown code page
    {
        QJsonObject j = makeBase();
        j["codePage"] = 999999;
        SessionConfig cfg;
        QVERIFY(!cfg.fromJson(j));
    }
    // Sanity: the base with a known code page succeeds
    {
        QJsonObject j = makeBase();
        j["codePage"] = 37;
        SessionConfig cfg;
        QVERIFY(cfg.fromJson(j));
        QCOMPARE(cfg.codePage(), core::CodePage::ID::CP037);
    }
}

void TestSessionConfig::testAllowInvalidCertificatesDefaultsOff() {
    SessionConfig fresh;
    QVERIFY(!fresh.allowInvalidCertificates());

    QJsonObject json;
    json["name"] = "Secure";
    json["hostname"] = "example.com";
    json["port"] = 992;
    json["useTLS"] = true;
    json["deviceType"] = "IBM-3179-2";
    json["deviceName"] = "TERM";
    json["screenRows"] = 24;
    json["screenCols"] = 80;
    SessionConfig loaded;
    QVERIFY(loaded.fromJson(json));
    QVERIFY(!loaded.allowInvalidCertificates());

    QJsonObject out = loaded.toJson();
    QVERIFY(!out.contains("allowInvalidCertificates"));
}

void TestSessionConfig::testAllowInvalidCertificatesRoundTrip() {
    m_config->setName("Legacy AS400");
    m_config->setHostname("legacy.example.com");
    m_config->setPort(992);
    m_config->setUseTLS(true);
    m_config->setAllowInvalidCertificates(true);

    QJsonObject json = m_config->toJson();
    QCOMPARE(json.value("allowInvalidCertificates").toBool(), true);

    SessionConfig loaded;
    QVERIFY(loaded.fromJson(json));
    QCOMPARE(loaded.allowInvalidCertificates(), true);

    QJsonObject reset;
    reset["name"] = "Reset";
    reset["hostname"] = "a.example.com";
    reset["port"] = 23;
    reset["useTLS"] = false;
    reset["deviceType"] = "IBM-3179-2";
    reset["deviceName"] = "T";
    reset["screenRows"] = 24;
    reset["screenCols"] = 80;
    SessionConfig second;
    second.setAllowInvalidCertificates(true);
    QVERIFY(second.fromJson(reset));
    QVERIFY(!second.allowInvalidCertificates());
}

void TestSessionConfig::testPcCommandPolicyDefaults() {
    // The default for a fresh SessionConfig must be DenyAndAlert: never run a
    // host-issued PC command without explicit opt-in, but always make refused
    // attempts visible so the user knows a host tried.
    SessionConfig fresh;
    QCOMPARE(fresh.pcCommandPolicy(), PcCommandPolicy::DenyAndAlert);

    // A JSON object that does not carry the policy key must also resolve to
    // the default. This covers existing configs written by an earlier build
    // (where the field did not exist) and any third-party config that omits
    // the field entirely.
    QJsonObject json;
    json["name"] = "Default";
    json["hostname"] = "example.com";
    json["port"] = 23;
    json["useTLS"] = false;
    json["deviceType"] = "IBM-3179-2";
    json["deviceName"] = "T";
    json["screenRows"] = 24;
    json["screenCols"] = 80;
    SessionConfig loaded;
    QVERIFY(loaded.fromJson(json));
    QCOMPARE(loaded.pcCommandPolicy(), PcCommandPolicy::DenyAndAlert);

    // Round-tripping a config at the default must NOT write the policy key,
    // so existing config files do not gain a surprising new key on first save.
    QJsonObject out = loaded.toJson();
    QVERIFY(!out.contains("pcCommandPolicy"));
    QVERIFY(!out.contains("pcCommandEnabled"));
    QVERIFY(!out.contains("pcCommandConfirmEachTime"));
}

void TestSessionConfig::testPcCommandPolicyRoundTrip() {
    // Each non-default policy must round-trip through JSON with a stable
    // string tag, and the loader must parse that tag back to the same enum.
    struct Case {
        PcCommandPolicy policy;
        const char *expectedTag;
    };
    const Case cases[] = {
        {PcCommandPolicy::Deny,            "deny"},
        {PcCommandPolicy::AllowWithPrompt, "allowPrompt"},
        {PcCommandPolicy::AllowAlways,     "allowAlways"},
    };

    for (const auto &c : cases) {
        SessionConfig src;
        src.setName("Round-trip");
        src.setHostname("h.example.com");
        src.setPort(23);
        src.setPcCommandPolicy(c.policy);

        QJsonObject json = src.toJson();
        QVERIFY2(json.contains("pcCommandPolicy"), c.expectedTag);
        QCOMPARE(json.value("pcCommandPolicy").toString(), QString::fromLatin1(c.expectedTag));

        SessionConfig loaded;
        QVERIFY(loaded.fromJson(json));
        QCOMPARE(loaded.pcCommandPolicy(), c.policy);
    }

    // A subsequent fromJson with NO policy key must reset back to the default
    // — guards against a stale policy leaking across config loads.
    QJsonObject reset;
    reset["name"] = "Reset";
    reset["hostname"] = "a.example.com";
    reset["port"] = 23;
    reset["useTLS"] = false;
    reset["deviceType"] = "IBM-3179-2";
    reset["deviceName"] = "T";
    reset["screenRows"] = 24;
    reset["screenCols"] = 80;
    SessionConfig stale;
    stale.setPcCommandPolicy(PcCommandPolicy::AllowAlways);
    QVERIFY(stale.fromJson(reset));
    QCOMPARE(stale.pcCommandPolicy(), PcCommandPolicy::DenyAndAlert);

    // Unknown policy strings must fall back to the safe default rather than
    // silently mapping to Deny — the alert path makes the bad value visible.
    QJsonObject unknown = reset;
    unknown["pcCommandPolicy"] = "totallyMadeUp";
    SessionConfig recovered;
    recovered.setPcCommandPolicy(PcCommandPolicy::AllowAlways);
    QVERIFY(recovered.fromJson(unknown));
    QCOMPARE(recovered.pcCommandPolicy(), PcCommandPolicy::DenyAndAlert);
}

void TestSessionConfig::testPcCommandPolicyLegacyConfigCompat() {
    // Configs written by the earlier draft of this PR carried a pair of
    // booleans (pcCommandEnabled + pcCommandConfirmEachTime). The loader must
    // still understand them so users who saved a config against that build
    // do not silently revert to the default after upgrade.
    QJsonObject base;
    base["name"] = "Legacy";
    base["hostname"] = "legacy.example.com";
    base["port"] = 23;
    base["useTLS"] = false;
    base["deviceType"] = "IBM-3179-2";
    base["deviceName"] = "T";
    base["screenRows"] = 24;
    base["screenCols"] = 80;

    {
        // pcCommandEnabled=true with no confirm flag -> AllowWithPrompt.
        QJsonObject j = base;
        j["pcCommandEnabled"] = true;
        SessionConfig cfg;
        QVERIFY(cfg.fromJson(j));
        QCOMPARE(cfg.pcCommandPolicy(), PcCommandPolicy::AllowWithPrompt);
    }
    {
        // pcCommandEnabled=true, pcCommandConfirmEachTime=false -> AllowAlways.
        QJsonObject j = base;
        j["pcCommandEnabled"] = true;
        j["pcCommandConfirmEachTime"] = false;
        SessionConfig cfg;
        QVERIFY(cfg.fromJson(j));
        QCOMPARE(cfg.pcCommandPolicy(), PcCommandPolicy::AllowAlways);
    }
    {
        // pcCommandEnabled=false (or absent) -> DenyAndAlert default. The old
        // build's "Deny silently" lands on the new "Deny and alert" default
        // intentionally — making refused attempts visible is the new floor.
        QJsonObject j = base;
        j["pcCommandEnabled"] = false;
        SessionConfig cfg;
        QVERIFY(cfg.fromJson(j));
        QCOMPARE(cfg.pcCommandPolicy(), PcCommandPolicy::DenyAndAlert);
    }
    {
        // The new policy key wins over the legacy bools when both are present.
        QJsonObject j = base;
        j["pcCommandEnabled"] = true;
        j["pcCommandConfirmEachTime"] = false;
        j["pcCommandPolicy"] = "deny";
        SessionConfig cfg;
        QVERIFY(cfg.fromJson(j));
        QCOMPARE(cfg.pcCommandPolicy(), PcCommandPolicy::Deny);
    }
}

void TestSessionConfig::testDistinctNamesDoNotCollideOnDisk() {
    QStandardPaths::setTestModeEnabled(true);

    const QString base = QStringLiteral("collision-")
                         + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString firstName = base + QStringLiteral("/profile");
    const QString secondName = base + QStringLiteral("?profile");

    SessionConfig first;
    first.setName(firstName);
    first.setHostname(QStringLiteral("first.example.com"));
    SessionConfig second;
    second.setName(secondName);
    second.setHostname(QStringLiteral("second.example.com"));

    SessionManager writer;
    QVERIFY(writer.saveSession(first));
    QVERIFY(writer.saveSession(second));

    SessionManager reader;
    QVERIFY(reader.sessionExists(firstName));
    QVERIFY(reader.sessionExists(secondName));

    SessionConfig loadedFirst;
    SessionConfig loadedSecond;
    QVERIFY(reader.loadSession(firstName, loadedFirst));
    QVERIFY(reader.loadSession(secondName, loadedSecond));
    QCOMPARE(loadedFirst.hostname(), QStringLiteral("first.example.com"));
    QCOMPARE(loadedSecond.hostname(), QStringLiteral("second.example.com"));

    QVERIFY(reader.deleteSession(firstName));
    QVERIFY(reader.deleteSession(secondName));
}

void TestSessionConfig::testLegacySessionFilenameLoads() {
    QStandardPaths::setTestModeEnabled(true);

    const QString name = QStringLiteral("legacy profile ")
                         + QUuid::createUuid().toString(QUuid::WithoutBraces);
    SessionConfig legacy;
    legacy.setName(name);
    legacy.setHostname(QStringLiteral("legacy.example.com"));

    QString legacyBase = name;
    legacyBase.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    const QString sessionsDir = QStandardPaths::writableLocation(
                                    QStandardPaths::AppDataLocation)
                                + QStringLiteral("/sessions");
    QVERIFY(QDir().mkpath(sessionsDir));
    QFile file(sessionsDir + QStringLiteral("/") + legacyBase
               + QStringLiteral(".json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QJsonDocument(legacy.toJson()).toJson()) > 0);
    file.close();

    SessionManager reader;
    QVERIFY(reader.sessionExists(name));
    SessionConfig loaded;
    QVERIFY(reader.loadSession(name, loaded));
    QCOMPARE(loaded.hostname(), QStringLiteral("legacy.example.com"));
    QVERIFY(reader.deleteSession(name));
}

// Make the enum visible to QCOMPARE's diagnostics (the registration is only
// for nicer failure messages — equality already works on plain enums).
Q_DECLARE_METATYPE(PcCommandPolicy)

QTEST_MAIN(TestSessionConfig)
#include "test_session_config.moc"
