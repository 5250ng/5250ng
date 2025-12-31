#include "logger.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace core {

Logger *Logger::s_instance = nullptr;

Logger::Logger(QObject *parent)
    : QObject(parent), m_logFile(nullptr), m_logStream(nullptr),
      m_logLevel(LogLevel::Info), m_consoleOutput(true) {
  // Set default log file
  QString appDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir;
  dir.mkpath(appDataDir);
  QString logPath = appDataDir + "/tn5250.log";
  setLogFile(logPath);
}

Logger::~Logger() {
  if (m_logStream) {
    m_logStream->flush();
    delete m_logStream;
  }
  if (m_logFile) {
    m_logFile->close();
    delete m_logFile;
  }
}

Logger *Logger::instance() {
  if (!s_instance) {
    s_instance = new Logger();
  }
  return s_instance;
}

void Logger::setLogFile(const QString &filePath) {
  QMutexLocker locker(&m_mutex);

  if (m_logStream) {
    m_logStream->flush();
    delete m_logStream;
  }
  if (m_logFile) {
    m_logFile->close();
    delete m_logFile;
  }

  m_logFile = new QFile(filePath);
  if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append |
                      QIODevice::Text)) {
    m_logStream = new QTextStream(m_logFile);
  } else {
    qWarning() << "Logger: Cannot open log file:" << filePath;
    delete m_logFile;
    m_logFile = nullptr;
  }
}

void Logger::setLogLevel(LogLevel level) {
  QMutexLocker locker(&m_mutex);
  m_logLevel = level;
}

void Logger::log(LogLevel level, const QString &message) {
  if (level < m_logLevel) {
    return;
  }

  writeLog(level, message);
  emit logMessage(level, message);
}

void Logger::debug(const QString &message) { log(LogLevel::Debug, message); }

void Logger::info(const QString &message) { log(LogLevel::Info, message); }

void Logger::warning(const QString &message) {
  log(LogLevel::Warning, message);
}

void Logger::error(const QString &message) { log(LogLevel::Error, message); }

QString Logger::levelToString(LogLevel level) const {
  switch (level) {
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warning:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

void Logger::writeLog(LogLevel level, const QString &message) {
  QMutexLocker locker(&m_mutex);

  QString timestamp =
      QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
  QString levelStr = levelToString(level);
  QString logLine = QString("[%1] [%2] %3").arg(timestamp, levelStr, message);

  if (m_consoleOutput) {
    switch (level) {
    case LogLevel::Debug:
      qDebug() << logLine;
      break;
    case LogLevel::Info:
      qInfo() << logLine;
      break;
    case LogLevel::Warning:
      qWarning() << logLine;
      break;
    case LogLevel::Error:
      qCritical() << logLine;
      break;
    }
  }

  if (m_logStream) {
    *m_logStream << logLine << Qt::endl;
    m_logStream->flush();
  }
}

} // namespace core
