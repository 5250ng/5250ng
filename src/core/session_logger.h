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
