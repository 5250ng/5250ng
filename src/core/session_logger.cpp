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

#include "session_logger.h"
#include <QDateTime>

namespace core {

SessionLogger::SessionLogger(QObject *parent) : QObject(parent) {}

SessionLogger::~SessionLogger() {
    stop();
}

bool SessionLogger::start(const QString &filePath, Verbosity verbosity) {
    // Stop outside the lock to avoid recursive locking deadlock
    if (m_active) stop();

    QMutexLocker lock(&m_mutex);

    m_filePath = filePath;
    m_verbosity = verbosity;
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;

    m_stream.setDevice(&m_file);
    m_active = true;
    m_screenCount = 0;

    m_stream << "=== Session log started: "
             << QDateTime::currentDateTime().toString(Qt::ISODate) << " ===" << Qt::endl;
    return true;
}

void SessionLogger::stop() {
    QMutexLocker lock(&m_mutex);
    if (!m_active) return;

    m_stream << "=== Session log ended: "
             << QDateTime::currentDateTime().toString(Qt::ISODate)
             << " (" << m_screenCount << " screens) ===" << Qt::endl;
    m_stream.flush();
    m_file.close();
    m_active = false;
}

void SessionLogger::logScreenTransition(const QString &screenText, int rows, int cols) {
    if (!m_active) return;
    QMutexLocker lock(&m_mutex);
    ++m_screenCount;

    m_stream << "\n--- Screen #" << m_screenCount << " ["
             << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
             << "] " << rows << "x" << cols << " ---" << Qt::endl;

    // Write screen content row by row
    for (int r = 0; r < rows && r * cols < screenText.size(); ++r) {
        m_stream << screenText.mid(r * cols, cols) << Qt::endl;
    }
}

void SessionLogger::logKeystroke(const QString &description) {
    if (!m_active || m_verbosity < ScreensAndKeys) return;
    writeLine(QString("[KEY] %1").arg(description));
}

void SessionLogger::logAIDKey(const QString &keyName) {
    if (!m_active) return;
    writeLine(QString("[AID] %1").arg(keyName));
}

void SessionLogger::logProtocolData(const QByteArray &data, bool inbound) {
    if (!m_active || m_verbosity < FullProtocol) return;
    writeLine(QString("[%1] %2 bytes: %3")
        .arg(inbound ? "RECV" : "SEND")
        .arg(data.size())
        .arg(QString(data.toHex(' '))));
}

void SessionLogger::logEvent(const QString &event) {
    if (!m_active) return;
    writeLine(QString("[EVENT] %1").arg(event));
}

void SessionLogger::writeLine(const QString &line) {
    QMutexLocker lock(&m_mutex);
    m_stream << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
             << " " << line << Qt::endl;
}

} // namespace core
