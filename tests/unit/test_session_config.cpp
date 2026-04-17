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
    void testAllowInvalidCertificatesDefaultsOff();
    void testAllowInvalidCertificatesRoundTrip();

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

QTEST_MAIN(TestSessionConfig)
#include "test_session_config.moc"
