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

#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QElapsedTimer>

namespace core {

struct MacroStep {
    enum Type { KeyPress, AIDKey, Delay };
    Type type;
    int key = 0;                // Qt::Key
    Qt::KeyboardModifiers mods; // Modifier keys
    QString text;               // Character text for key press
    uint8_t aidByte = 0;       // For AID key steps
    int delayMs = 0;           // For delay steps

    QJsonObject toJson() const;
    static MacroStep fromJson(const QJsonObject &obj);
};

struct Macro {
    QString name;
    QString description;
    QString filePath; // On-disk path (empty for unsaved/in-memory macros)
    QVector<MacroStep> steps;

    QJsonObject toJson() const;
    static Macro fromJson(const QJsonObject &obj);
};

class MacroRecorder : public QObject {
    Q_OBJECT

  public:
    explicit MacroRecorder(QObject *parent = nullptr);

    bool isRecording() const { return m_recording; }
    bool isPlaying() const { return m_playing; }

    void startRecording();
    void stopRecording();
    Macro finishRecording(const QString &name);
    void discardRecording();

    void recordKeyPress(int key, Qt::KeyboardModifiers mods, const QString &text);
    void recordAIDKey(uint8_t aidByte);

    // Async playback with real delays
    void play(const Macro &macro, int repeatCount = 1);
    void stopPlayback();

    // Notify the recorder that the keyboard has been unlocked by the host.
    // During playback after an AID key, execution pauses until this is called.
    void notifyKeyboardUnlocked();

    // Access last recorded macro (for quick replay)
    const Macro &lastRecordedMacro() const { return m_lastRecorded; }

    // Persistence
    static bool saveMacro(const Macro &macro, const QString &filePath);
    static Macro loadMacro(const QString &filePath);
    static QVector<Macro> loadAllMacros(const QString &directory);

    // Filename sanitization
    static QString sanitizeFileName(const QString &name);

  signals:
    void recordingStarted();
    void recordingStopped();
    void playbackStarted();
    void playbackFinished();
    void playbackStep(const MacroStep &step);

  private slots:
    void executeNextStep();

  private:
    bool m_recording = false;
    bool m_playing = false;
    QVector<MacroStep> m_steps;
    QElapsedTimer m_timer;
    qint64 m_lastStepTime = 0;

    // Playback state
    Macro m_playbackMacro;
    int m_playbackIndex = 0;
    QTimer m_playbackTimer;
    bool m_waitingForUnlock = false;
    int m_repeatCount = 1;
    int m_currentRepeat = 0;

    // Last recorded macro for quick replay
    Macro m_lastRecorded;
};

} // namespace core
