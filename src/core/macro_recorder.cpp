#include "macro_recorder.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>

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

void MacroRecorder::recordKeyPress(int key, Qt::KeyboardModifiers mods, const QString &text) {
    if (!m_recording) return;

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

void MacroRecorder::play(const Macro &macro) {
    if (m_playing) return;
    m_playbackMacro = macro;
    m_playbackIndex = 0;
    m_playing = true;
    emit playbackStarted();
    executeNextStep();
}

void MacroRecorder::executeNextStep() {
    if (!m_playing || m_playbackIndex >= m_playbackMacro.steps.size()) {
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

    // Schedule the next step immediately (0ms timer to yield to event loop)
    QTimer::singleShot(0, this, &MacroRecorder::executeNextStep);
}

void MacroRecorder::stopPlayback() {
    m_playing = false;
    m_playbackTimer.stop();
}

bool MacroRecorder::saveMacro(const Macro &macro, const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QJsonDocument doc(macro.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

Macro MacroRecorder::loadMacro(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return Macro{};
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return Macro::fromJson(doc.object());
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

} // namespace core
