#pragma once

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace logger {

// Log levels
/**
 * Describes the severity of a log entry.
 *
 * - Debug: Verbose diagnostic information for development and troubleshooting.
 * - Info: Informational events that highlight progress of the application.
 * - Warning: Potentially harmful situations that do not stop the program.
 * - Error: Error events that likely require attention, potentially preventing normal flow.
 */
enum class LogLevel { Debug,
                      Info,
                      Warning,
                      Error };

// Logger for 5250ng
/**
 * Simple thread-safe logger with pluggable output backends.
 *
 * Features:
 * - Singleton access via instance()
 * - Configurable minimum log level
 * - Optional console output
 * - Optional file output (append mode)
 * - Qt signal emission for UI subscribers
 *
 * Typical usage:
 *   logger::Logger::instance()->setLogLevel(logger::LogLevel::Debug);
 *   logger::Logger::instance()->enableConsoleOutput(true);
 *   logger::Logger::instance()->debug("Starting app");
 */
class Logger : public QObject {
    Q_OBJECT

  public:
    /**
     * Get the global singleton instance of the Logger.
     * Thread-safe on first call under Qt object creation semantics.
     */
    static Logger *instance();

    /**
     * Log a message with the provided severity level.
     * If level is lower than the current configured minimum, the message is ignored.
     *
     * @param level   Severity of the message.
     * @param message Message to log.
     */
    void log(LogLevel level, const QString &message);
    /**
     * Log a message with a specific severity level and a prefix.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void log_with_prefix(LogLevel level, const QString &prefix, const QString &message);
    /**
     * Convenience method to log a Debug-level message.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void debug(const QString &message);
    /**
     * Convenience method to log a Debug-level message with a prefix.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void debug_with_prefix(const QString &prefix, const QString &message);
    /**
     * Convenience method to log an Info-level message.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void info(const QString &message);
    /**
     * Convenience method to log an Info-level message with a prefix.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void info_with_prefix(const QString &prefix, const QString &message);
    /**
     * Convenience method to log a Warning-level message.
     */
    void warning(const QString &message);
    /**
     * Convenience method to log a Warning-level message with a prefix.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void warning_with_prefix(const QString &prefix, const QString &message);
    /**
     * Convenience method to log an Error-level message.
     */
    void error(const QString &message);
    /**
     * Convenience method to log an Error-level message with a prefix.
     * If the message contains newline characters, each line is logged separately
     * with its own timestamp/level prefix.
     */
    void error_with_prefix(const QString &prefix, const QString &message);

    /**
     * Set the file where logs will be appended.
     * If the file cannot be opened, logs will continue without file output.
     *
     * @param filePath Absolute or relative path to a writable log file.
     */
    void setLogFile(const QString &filePath);
    /**
     * Set the minimum severity level to be recorded.
     * Messages below this level are discarded.
     *
     * @param level New minimum log level.
     */
    void setLogLevel(LogLevel level);
    /**
     * Get the currently configured minimum log level.
     */
    LogLevel logLevel() const { return m_logLevel; }

    /**
     * Enable or disable console (stderr/stdout) output in addition to file output.
     *
     * @param enable True to print to console, false to disable console output.
     */
    void enableConsoleOutput(bool enable) { m_consoleOutput = enable; }

    /**
     * Return the current log file path, or empty if no file is configured.
     */
    QString logFilePath() const { return m_logFile ? m_logFile->fileName() : QString(); }

  signals:
    /**
     * Emitted whenever a message is logged (and passes the minimum level filter).
     *
     * @param level   Severity of the message.
     * @param message Logged message (already formatted with timestamp/level in writeLog()).
     */
    void logMessage(LogLevel level, const QString &message);

  private:
    /**
     * Construct a Logger. Use instance() instead.
     * Initializes default log file under platform AppDataLocation.
     */
    Logger(QObject *parent = nullptr);
    /**
     * Destructor flushes and closes open file resources.
     */
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    /**
     * Convert a LogLevel value to its printable short string representation.
     */
    QString levelToString(LogLevel level) const;
    /**
     * Core output routine that formats and writes the message to console and/or file.
     * Thread-safe via internal mutex.
     */
    void writeLog(LogLevel level, const QString &message);

    QFile *m_logFile;
    QTextStream *m_logStream;
    QMutex m_mutex;
    LogLevel m_logLevel;
    bool m_consoleOutput;
};

// Convenience macros
/**
 * Shorthand macros for logging at specific levels.
 * Prefer these for brevity in call sites.
 */
#define LOG_DEBUG(msg) logger::Logger::instance()->debug(msg)
#define LOG_INFO(msg) logger::Logger::instance()->info(msg)
#define LOG_WARNING(msg) logger::Logger::instance()->warning(msg)
#define LOG_ERROR(msg) logger::Logger::instance()->error(msg)

} // namespace logger
