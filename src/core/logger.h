#pragma once

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace core {

// Log levels
enum class LogLevel { Debug, Info, Warning, Error };

// Logger for 5250ng
class Logger : public QObject {
  Q_OBJECT

public:
  static Logger *instance();

  void log(LogLevel level, const QString &message);
  void debug(const QString &message);
  void info(const QString &message);
  void warning(const QString &message);
  void error(const QString &message);

  void setLogFile(const QString &filePath);
  void setLogLevel(LogLevel level);
  LogLevel logLevel() const { return m_logLevel; }

  void enableConsoleOutput(bool enable) { m_consoleOutput = enable; }

signals:
  void logMessage(LogLevel level, const QString &message);

private:
  Logger(QObject *parent = nullptr);
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  QString levelToString(LogLevel level) const;
  void writeLog(LogLevel level, const QString &message);

  static Logger *s_instance;
  QFile *m_logFile;
  QTextStream *m_logStream;
  QMutex m_mutex;
  LogLevel m_logLevel;
  bool m_consoleOutput;
};

// Convenience macros
#define LOG_DEBUG(msg) core::Logger::instance()->debug(msg)
#define LOG_INFO(msg) core::Logger::instance()->info(msg)
#define LOG_WARNING(msg) core::Logger::instance()->warning(msg)
#define LOG_ERROR(msg) core::Logger::instance()->error(msg)

} // namespace core
