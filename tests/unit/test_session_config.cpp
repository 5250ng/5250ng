#include "core/session_config.h"
#include <QtTest/QtTest>

using namespace core;

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
  QCOMPARE(m_config->deviceName(), QString("QT5250"));
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
  json["deviceName"] = "LOADED5250";
  json["screenRows"] = 27;
  json["screenCols"] = 132;

  SessionConfig loaded;
  QVERIFY(loaded.fromJson(json));

  QCOMPARE(loaded.name(), QString("Loaded Session"));
  QCOMPARE(loaded.hostname(), QString("test.example.com"));
  QCOMPARE(loaded.port(), static_cast<quint16>(992));
  QCOMPARE(loaded.useTLS(), true);
  QCOMPARE(loaded.deviceName(), QString("LOADED5250"));
  QCOMPARE(loaded.screenRows(), 27);
  QCOMPARE(loaded.screenCols(), 132);
}

QTEST_MAIN(TestSessionConfig)
#include "test_session_config.moc"
