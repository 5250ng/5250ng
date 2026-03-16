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

#include "macro_recorder.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>

namespace core {

// --- MacroStep ---

QJsonObject MacroStep::toJson() const {
    QJsonObject obj;
    switch (type) {
    case KeyPress:
        obj["type"] = "keypress";
        obj["key"] = key;
        obj["mods"] = static_cast<int>(mods.toInt());
        if (!text.isEmpty()) obj["text"] = text;
        break;
    case AIDKey:
        obj["type"] = "aid";
        obj["aid"] = aidByte;
        break;
    case Delay:
        obj["type"] = "delay";
        obj["ms"] = delayMs;
        break;
    }
    return obj;
}

MacroStep MacroStep::fromJson(const QJsonObject &obj) {
    MacroStep step;
    QString type = obj["type"].toString();
    if (type == "keypress") {
        step.type = KeyPress;
        step.key = obj["key"].toInt();
        step.mods = Qt::KeyboardModifiers::fromInt(obj["mods"].toInt());
        step.text = obj["text"].toString();
    } else if (type == "aid") {
        step.type = AIDKey;
        step.aidByte = static_cast<uint8_t>(obj["aid"].toInt());
    } else if (type == "delay") {
        step.type = Delay;
        step.delayMs = obj["ms"].toInt();
    }
    return step;
}

// --- Macro ---

QJsonObject Macro::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["description"] = description;
    QJsonArray arr;
    for (const auto &step : steps)
        arr.append(step.toJson());
    obj["steps"] = arr;
    return obj;
}

Macro Macro::fromJson(const QJsonObject &obj) {
    Macro m;
    m.name = obj["name"].toString();
    m.description = obj["description"].toString();
    for (const auto &val : obj["steps"].toArray())
        m.steps.append(MacroStep::fromJson(val.toObject()));
    return m;
}

// --- MacroRecorder ---

MacroRecorder::MacroRecorder(QObject *parent) : QObject(parent) {
    m_playbackTimer.setSingleShot(true);
    connect(&m_playbackTimer, &QTimer::timeout, this, &MacroRecorder::executeNextStep);
}

void MacroRecorder::startRecording() {
    if (m_playing) return; // Bug fix #3: prevent recording during playback
    m_steps.clear();
    m_recording = true;
    m_timer.start();
    m_lastStepTime = 0;
    emit recordingStarted();
}

void MacroRecorder::stopRecording() {
    m_recording = false;
    emit recordingStopped();
}

Macro MacroRecorder::finishRecording(const QString &name) {
    Macro macro;
    macro.name = name;
    macro.steps = m_steps;
    m_steps.clear();
    m_lastRecorded = macro;
    return macro;
}

void MacroRecorder::discardRecording() {
    m_steps.clear();
}

void MacroRecorder::recordKeyPress(int key, Qt::KeyboardModifiers mods, const QString &text) {
    if (!m_recording) return;

    // Ignore bare modifier keys — they are not actionable steps
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
        return;
    }

    // Insert delay between steps to preserve timing
    qint64 now = m_timer.elapsed();
    if (m_lastStepTime > 0) {
        int gap = static_cast<int>(now - m_lastStepTime);
        if (gap > 50) {  // Record delays > 50ms for faithful replay
            MacroStep delay;
            delay.type = MacroStep::Delay;
            delay.delayMs = gap;
            m_steps.append(delay);
        }
    }
    m_lastStepTime = now;

    MacroStep step;
    step.type = MacroStep::KeyPress;
    step.key = key;
    step.mods = mods;
    step.text = text;
    m_steps.append(step);
}

void MacroRecorder::recordAIDKey(uint8_t aidByte) {
    if (!m_recording) return;

    qint64 now = m_timer.elapsed();
    if (m_lastStepTime > 0) {
        int gap = static_cast<int>(now - m_lastStepTime);
        if (gap > 50) {
            MacroStep delay;
            delay.type = MacroStep::Delay;
            delay.delayMs = gap;
            m_steps.append(delay);
        }
    }
    m_lastStepTime = now;

    MacroStep step;
    step.type = MacroStep::AIDKey;
    step.aidByte = aidByte;
    m_steps.append(step);
}

void MacroRecorder::play(const Macro &macro, int repeatCount) {
    if (m_playing || m_recording) return; // Bug fix #3: prevent play during recording
    m_playbackMacro = macro;
    m_playbackIndex = 0;
    m_playing = true;
    m_waitingForUnlock = false;
    m_repeatCount = qMax(1, repeatCount);
    m_currentRepeat = 0;
    emit playbackStarted();
    executeNextStep();
}

void MacroRecorder::executeNextStep() {
    if (!m_playing) return;

    // Safety timeout fired while waiting for keyboard unlock - abort playback
    if (m_waitingForUnlock) {
        m_playing = false;
        m_waitingForUnlock = false;
        emit playbackFinished();
        return;
    }

    if (m_playbackIndex >= m_playbackMacro.steps.size()) {
        // End of one iteration - check if we need to repeat
        ++m_currentRepeat;
        if (m_currentRepeat < m_repeatCount) {
            m_playbackIndex = 0;
            QTimer::singleShot(0, this, &MacroRecorder::executeNextStep);
            return;
        }
        m_playing = false;
        emit playbackFinished();
        return;
    }

    const MacroStep &step = m_playbackMacro.steps[m_playbackIndex];
    ++m_playbackIndex;

    if (step.type == MacroStep::Delay) {
        // Wait the recorded delay, then execute the next step
        m_playbackTimer.start(step.delayMs);
        return;
    }

    // Emit the step for the main window to feed into the terminal
    emit playbackStep(step);

    // After an AID key, pause and wait for the host to unlock the keyboard
    // instead of blindly continuing with recorded delays
    if (step.type == MacroStep::AIDKey) {
        m_waitingForUnlock = true;
        // Safety timeout: if no unlock arrives in 30s, abort playback
        m_playbackTimer.start(30000);
        return;
    }

    // Schedule the next step immediately (0ms timer to yield to event loop)
    QTimer::singleShot(0, this, &MacroRecorder::executeNextStep);
}

void MacroRecorder::notifyKeyboardUnlocked() {
    if (!m_playing || !m_waitingForUnlock) return;
    m_waitingForUnlock = false;
    m_playbackTimer.stop();
    // Small delay to let the screen settle before continuing
    QTimer::singleShot(50, this, &MacroRecorder::executeNextStep);
}

void MacroRecorder::stopPlayback() {
    m_playing = false;
    m_waitingForUnlock = false;
    m_playbackTimer.stop();
}

bool MacroRecorder::saveMacro(const Macro &macro, const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QJsonDocument doc(macro.toJson());
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    return file.write(data) == data.size();
}

Macro MacroRecorder::loadMacro(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return Macro{};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    Macro m = Macro::fromJson(doc.object());
    m.filePath = filePath;
    return m;
}

QVector<Macro> MacroRecorder::loadAllMacros(const QString &directory) {
    QVector<Macro> macros;
    QDir dir(directory);
    for (const auto &entry : dir.entryInfoList({"*.json", "*.macro"}, QDir::Files)) {
        Macro m = loadMacro(entry.absoluteFilePath());
        if (!m.name.isEmpty())
            macros.append(m);
    }
    return macros;
}

QString MacroRecorder::sanitizeFileName(const QString &name) {
    QString safe = name;
    // Remove characters that are invalid in filenames across platforms
    static QRegularExpression re(R"([<>:"/\\|?*\x00-\x1F])");
    safe.replace(re, "_");
    // Remove leading/trailing dots and spaces
    safe = safe.trimmed();
    while (safe.startsWith('.')) safe.remove(0, 1);
    while (safe.endsWith('.')) safe.chop(1);
    safe = safe.trimmed();
    if (safe.isEmpty()) safe = "macro";
    // Truncate to reasonable length
    if (safe.length() > 200) safe.truncate(200);
    return safe;
}

} // namespace core
