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

#include "logger.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>

namespace logger {

/**
 * Construct a Logger with defaults:
 * - Appends to a log file in the platform AppDataLocation
 * - Console output enabled
 * - Minimum log level set to Info
 */
Logger::Logger(QObject *parent)
    : QObject(parent), m_logFile(nullptr), m_logStream(nullptr),
      m_logLevel(LogLevel::Info), m_consoleOutput(true) {
    // File logging is off by default (use --debug to enable).
    // Logs are always kept in the in-memory ring buffer.
}

/**
 * Destructor flushes and closes any open log file resources.
 */
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

/**
 * Return the singleton logger instance, creating it on first use.
 * Uses Meyers singleton pattern for thread-safe initialization.
 */
Logger *Logger::instance() {
    static Logger instance;
    return &instance;
}

/**
 * Configure the output log file. Opens the file in append mode.
 * If opening fails, logs will continue without file output and a warning is printed.
 */
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
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream = new QTextStream(m_logFile);
    } else {
        qWarning() << "Logger: Cannot open log file:" << filePath;
        delete m_logFile;
        m_logFile = nullptr;
    }
}

/**
 * Set the minimum severity level to be recorded.
 */
void Logger::setLogLevel(LogLevel level) {
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

/**
 * Log a message with a specific severity level.
 * Emits the logMessage signal after writing.
 */
void Logger::log(LogLevel level, const QString &message) {
    if (level < m_logLevel) {
        return;
    }

    // Split multiline messages and log each line independently
    if (message.contains('\n')) {
        const QStringList lines = message.trimmed().split('\n', Qt::KeepEmptyParts);
        for (QString line : lines) {
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            writeLog(level, line);
            emit logMessage(level, line);
        }
    } else {
        writeLog(level, message);
        emit logMessage(level, message);
    }

    return;
}

/**
 * Log a message with a specific severity level.
 * Emits the logMessage signal after writing.
 */
void Logger::log_with_prefix(LogLevel level, const QString &prefix, const QString &message) {
    if (level < m_logLevel) {
        return;
    }

    // Split multiline messages and log each line independently
    if (message.contains('\n')) {
        const QStringList lines = message.trimmed().split('\n', Qt::KeepEmptyParts);
        for (QString line : lines) {
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            writeLog(level, QString("%1: %2").arg(prefix, line));
            emit logMessage(level, QString("%1: %2").arg(prefix, line));
        }
    } else {
        writeLog(level, QString("%1: %2").arg(prefix, message));
        emit logMessage(level, QString("%1: %2").arg(prefix, message));
    }

    return;
}

/**
 * Convenience wrapper for Debug-level logging.
 */
void Logger::debug(const QString &message) { log(LogLevel::Debug, message); }

/**
 * Convenience wrapper for Debug-level logging.
 */
void Logger::debug_with_prefix(const QString &prefix, const QString &message) { log_with_prefix(LogLevel::Debug, prefix, message); }

/**
 * Convenience wrapper for Info-level logging.
 */
void Logger::info(const QString &message) { log(LogLevel::Info, message); }

/**
 * Convenience wrapper for Info-level logging.
 */
void Logger::info_with_prefix(const QString &prefix, const QString &message) { log_with_prefix(LogLevel::Info, prefix, message); }

/**
 * Convenience wrapper for Warning-level logging.
 */
void Logger::warning(const QString &message) {
    log(LogLevel::Warning, message);
}

/**
 * Convenience wrapper for Warning-level logging.
 */
void Logger::warning_with_prefix(const QString &prefix, const QString &message) { log_with_prefix(LogLevel::Warning, prefix, message); }

/**
 * Convenience wrapper for Error-level logging.
 */
void Logger::error(const QString &message) { log(LogLevel::Error, message); }

/**
 * Convenience wrapper for Error-level logging.
 */
void Logger::error_with_prefix(const QString &prefix, const QString &message) { log_with_prefix(LogLevel::Error, prefix, message); }

/**
 * Convert a LogLevel to its short printable string.
 */
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

/**
 * Compose and write a log line to console and/or file in a thread-safe way.
 * Format: [YYYY-mm-dd HH:MM:SS.mmm] [LEVEL] message
 */
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

    // In-memory ring buffer (always active)
    m_buffer.append(logLine);
    if (m_buffer.size() > MAX_BUFFER) {
        m_buffer.erase(m_buffer.begin(), m_buffer.end() - MAX_BUFFER);
    }
}

QStringList Logger::recentLogs() const {
    QMutexLocker locker(&m_mutex);
    return m_buffer;
}

} // namespace logger
