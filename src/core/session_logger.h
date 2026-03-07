#pragma once

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace core {

class SessionLogger : public QObject {
    Q_OBJECT

  public:
    enum Verbosity { ScreensOnly, ScreensAndKeys, FullProtocol };

    explicit SessionLogger(QObject *parent = nullptr);
    ~SessionLogger();

    bool start(const QString &filePath, Verbosity verbosity = ScreensAndKeys);
    void stop();
    bool isActive() const { return m_active; }

    void logScreenTransition(const QString &screenText, int rows, int cols);
    void logKeystroke(const QString &description);
    void logAIDKey(const QString &keyName);
    void logProtocolData(const QByteArray &data, bool inbound);
    void logEvent(const QString &event);

    Verbosity verbosity() const { return m_verbosity; }
    QString filePath() const { return m_filePath; }

  private:
    void writeLine(const QString &line);

    bool m_active = false;
    Verbosity m_verbosity = ScreensAndKeys;
    QString m_filePath;
    QFile m_file;
    QTextStream m_stream;
    QMutex m_mutex;
    int m_screenCount = 0;
};

} // namespace core
