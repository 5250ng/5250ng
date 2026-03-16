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

#include "script_executor.h"
#include "core/ebcdic.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QApplication>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <algorithm>

namespace core::scripting {

ScriptExecutor::ScriptExecutor(QObject *parent)
    : QObject(parent) {
    m_expectTimer.setSingleShot(true);
    connect(&m_expectTimer, &QTimer::timeout, this, [this]() {
        endExpect(false);
    });
}

void ScriptExecutor::setScreenWidget(ui::widgets::Q5250ScreenWidget *widget) {
    m_screenWidget = widget;
}

ui::widgets::ScreenBuffer *ScriptExecutor::screenBuffer() const {
    return m_screenWidget ? m_screenWidget->screenBuffer() : nullptr;
}

void ScriptExecutor::execute(const ParseResult &parseResult) {
    if (m_running) return;

    m_parseResult = parseResult;
    m_running = true;
    m_variables.clear();
    m_execStack.clear();
    m_onTimeoutLabel.clear();
    m_onErrorLabel.clear();
    m_timeout = 30000;
    m_actionDelay = 0;
    m_jitterMin = 0;
    m_jitterMax = 0;
    m_waitingForUnlock = false;
    m_expectActive = false;
    m_functions = parseResult.functions;
    m_callStack.clear();

    // Initialize built-in variables
    setVariable("$EXPECT_RESULT", "OK");
    setVariable("$REPEAT_INDEX", "0");
    updateBuiltinVariables();

    // Push root frame
    m_execStack.append({&m_parseResult.root->children, 0, 0, 0});

    emit executionStarted();
    scheduleNextStep();
}

void ScriptExecutor::stop() {
    m_running = false;
    m_expectTimer.stop();
    m_expectActive = false;
    m_waitingForUnlock = false;
    m_execStack.clear();
    m_callStack.clear();
    emit executionFinished();
}

void ScriptExecutor::resumeAfterPause() {
    if (!m_running) return;
    scheduleNextStep();
}

void ScriptExecutor::notifyScreenChanged() {
    if (!m_running) return;
    updateBuiltinVariables();

    if (m_expectActive && m_expectNode) {
        if (checkExpectCondition(m_expectNode)) {
            endExpect(true);
        }
    }
}

void ScriptExecutor::notifyTerminalStateChanged() {
    if (!m_running) return;
    updateBuiltinVariables();

    // Check if we're waiting for keyboard unlock after an AID key
    if (m_waitingForUnlock && m_screenWidget) {
        auto ks = m_screenWidget->keyboardState();
        if (ks == ui::widgets::KeyboardState::Unlocked) {
            m_waitingForUnlock = false;
            scheduleNextStep(50); // Small delay after unlock
        } else if (ks == ui::widgets::KeyboardState::ErrorLocked) {
            // Terminal entered error-locked state — trigger ON ERROR handler if set
            m_waitingForUnlock = false;
            if (!m_onErrorLabel.isEmpty()) {
                gotoLabel(m_onErrorLabel);
                scheduleNextStep();
            } else {
                emit executionError(0, "Terminal entered error-locked state");
                stop();
            }
            return;
        }
    }

    // Check EXPECT conditions that depend on terminal state
    if (m_expectActive && m_expectNode) {
        if (checkExpectCondition(m_expectNode)) {
            endExpect(true);
        }
    }
}

void ScriptExecutor::scheduleNextStep(int delayMs) {
    if (!m_running) return;
    int effectiveDelay = std::max(delayMs, m_actionDelay);
    if (m_jitterMax > 0) {
        int jitter = m_jitterMin + (QRandomGenerator::global()->bounded(m_jitterMax - m_jitterMin + 1));
        effectiveDelay = std::max(effectiveDelay, jitter);
    }
    if (effectiveDelay > 0)
        QTimer::singleShot(effectiveDelay, this, &ScriptExecutor::executeStep);
    else
        QTimer::singleShot(0, this, &ScriptExecutor::executeStep);
}

void ScriptExecutor::executeStep() {
    if (!m_running) return;
    if (m_waitingForUnlock || m_expectActive) return;

    // Find next node to execute
    while (!m_execStack.isEmpty()) {
        auto &frame = m_execStack.last();
        if (frame.index < frame.nodes->size()) {
            auto node = (*frame.nodes)[frame.index];
            frame.index++;
            executeNode(node);
            return;
        }

        // Frame exhausted
        // Check if this is a REPEAT block
        if (frame.repeatCount > 0) {
            frame.currentRepeat++;
            if (frame.currentRepeat < frame.repeatCount) {
                frame.index = 0;
                setVariable("$REPEAT_INDEX", QString::number(frame.currentRepeat));
                auto node = (*frame.nodes)[frame.index];
                frame.index++;
                executeNode(node);
                return;
            }
        }

        m_execStack.removeLast();

        // Implicit return: if we've popped back to the call frame's depth, return from function
        if (!m_callStack.isEmpty() &&
            m_execStack.size() == m_callStack.last().execStackDepth) {
            returnFromFunction();
        }
    }

    // All done
    m_running = false;
    emit executionFinished();
}

void ScriptExecutor::executeNode(const std::shared_ptr<ASTNode> &node) {
    if (!m_running) return;

    switch (node->type) {
    case NodeType::Label:
        scheduleNextStep();
        break;

    case NodeType::TypeInput: {
        QString text = interpolateVariables(node->stringValue);
        for (const QChar &ch : text) {
            emit injectKeyPress(0, Qt::NoModifier, QString(ch));
        }
        scheduleNextStep(10);
        break;
    }

    case NodeType::CharInput: {
        QString text = interpolateVariables(node->stringValue);
        if (!text.isEmpty())
            emit injectKeyPress(0, Qt::NoModifier, text.left(1));
        scheduleNextStep(10);
        break;
    }

    case NodeType::AIDKey: {
        emit injectAIDKey(node->aidByte);
        // After AID key, wait for keyboard unlock
        m_waitingForUnlock = true;
        // Don't schedule next step — notifyTerminalStateChanged will resume
        break;
    }

    case NodeType::LocalKey: {
        // Map local key names to Qt key events
        int qtKey = 0;
        Qt::KeyboardModifiers mods = Qt::NoModifier;
        const QString &name = node->stringValue;

        if (name == "TAB")            { qtKey = Qt::Key_Tab; }
        else if (name == "BACKTAB")   { qtKey = Qt::Key_Backtab; }
        else if (name == "BACKSPACE") { qtKey = Qt::Key_Backspace; }
        else if (name == "DELETE")    { qtKey = Qt::Key_Delete; }
        else if (name == "INSERT")    { qtKey = Qt::Key_Insert; }
        else if (name == "HOME")      { qtKey = Qt::Key_Home; }
        else if (name == "END")       { qtKey = Qt::Key_End; }
        else if (name == "ESC")       { qtKey = Qt::Key_Escape; }
        else if (name == "FIELDPLUS") { qtKey = Qt::Key_Plus; mods = Qt::KeypadModifier; }
        else if (name == "FIELDMINUS"){ qtKey = Qt::Key_Minus; mods = Qt::KeypadModifier; }
        else if (name == "FIELDEXIT") { qtKey = Qt::Key_Return; mods = Qt::ControlModifier; }
        else if (name == "DUP")       { qtKey = Qt::Key_D; mods = Qt::ControlModifier; }
        else if (name == "ERASEINPUT"){ qtKey = Qt::Key_A; mods = Qt::ControlModifier | Qt::ShiftModifier; }
        else if (name == "ERASEFIELD"){ qtKey = Qt::Key_Delete; mods = Qt::ControlModifier; }
        else if (name == "ERASEEOF")  { qtKey = Qt::Key_E; mods = Qt::ControlModifier; }

        if (qtKey != 0)
            emit injectKeyPress(qtKey, mods, QString());
        scheduleNextStep(10);
        break;
    }

    case NodeType::MoveCursorAt:
        switch (node->moveCursorMode) {
        case MoveCursorMode::Position:
            // Convert 1-based (script) to 0-based (internal)
            emit moveCursor(node->intValue - 1, node->intValue2 - 1);
            break;
        case MoveCursorMode::FieldIndex:
            emit gotoInputField(node->intValue);
            break;
        case MoveCursorMode::NextField:
            emit gotoInputField(-1);
            break;
        case MoveCursorMode::PreviousField:
            emit gotoInputField(-2);
            break;
        case MoveCursorMode::StepUp:
            for (int i = 0; i < node->intValue; ++i)
                emit moveCursorStep("UP");
            break;
        case MoveCursorMode::StepDown:
            for (int i = 0; i < node->intValue; ++i)
                emit moveCursorStep("DOWN");
            break;
        case MoveCursorMode::StepLeft:
            for (int i = 0; i < node->intValue; ++i)
                emit moveCursorStep("LEFT");
            break;
        case MoveCursorMode::StepRight:
            for (int i = 0; i < node->intValue; ++i)
                emit moveCursorStep("RIGHT");
            break;
        }
        scheduleNextStep();
        break;

    case NodeType::Wait:
        scheduleNextStep(node->intValue);
        break;

    case NodeType::GlobalDelay:
        m_actionDelay = node->intValue;
        scheduleNextStep();
        break;

    case NodeType::GlobalJitter:
        m_jitterMin = node->intValue;
        m_jitterMax = node->intValue2;
        scheduleNextStep();
        break;

    case NodeType::GlobalExpectTimeout:
        m_timeout = node->intValue;
        scheduleNextStep();
        break;

    case NodeType::Set: {
        QString value = interpolateVariables(node->stringValue);
        setVariable(node->varName, value);
        scheduleNextStep();
        break;
    }

    case NodeType::Inc: {
        QString val = resolveVariable(node->varName);
        setVariable(node->varName, QString::number(val.toInt() + 1));
        scheduleNextStep();
        break;
    }

    case NodeType::Dec: {
        QString val = resolveVariable(node->varName);
        setVariable(node->varName, QString::number(val.toInt() - 1));
        scheduleNextStep();
        break;
    }

    case NodeType::Add: {
        QString val = resolveVariable(node->varName);
        setVariable(node->varName, QString::number(val.toInt() + node->intValue));
        scheduleNextStep();
        break;
    }

    case NodeType::If: {
        QString left = interpolateVariables(node->condLeft);
        QString right = interpolateVariables(node->condRight);
        if (evaluateCondition(left, node->condOp, right)) {
            if (!node->children.isEmpty()) {
                m_execStack.append({&node->children, 0, 0, 0});
            }
        } else {
            if (!node->elseChildren.isEmpty()) {
                m_execStack.append({&node->elseChildren, 0, 0, 0});
            }
        }
        scheduleNextStep();
        break;
    }

    case NodeType::While: {
        QString left = interpolateVariables(node->condLeft);
        QString right = interpolateVariables(node->condRight);
        if (evaluateCondition(left, node->condOp, right)) {
            if (!node->children.isEmpty()) {
                // Decrement parent frame index so WHILE is re-executed after body completes
                if (!m_execStack.isEmpty()) {
                    m_execStack.last().index--;
                }
                m_execStack.append({&node->children, 0, 0, 0});
            }
        }
        // If condition is false, just continue (WHILE node consumed)
        scheduleNextStep();
        break;
    }

    case NodeType::Repeat: {
        if (!node->children.isEmpty() && node->intValue > 0) {
            setVariable("$REPEAT_INDEX", "0");
            m_execStack.append({&node->children, 0, node->intValue, 0});
        }
        scheduleNextStep();
        break;
    }

    case NodeType::FunctionCall: {
        if (!m_functions.contains(node->stringValue)) {
            emit executionError(node->line,
                                QString("Function '%1' not defined").arg(node->stringValue));
            stop();
            return;
        }
        if (m_callStack.size() >= 100) {
            emit executionError(node->line, "Maximum recursion depth (100) exceeded");
            stop();
            return;
        }
        auto &func = m_functions[node->stringValue];
        if (node->argValues.size() != func->paramNames.size()) {
            emit executionError(node->line,
                                QString("Function '%1' expects %2 arguments, got %3")
                                    .arg(node->stringValue)
                                    .arg(func->paramNames.size())
                                    .arg(node->argValues.size()));
            stop();
            return;
        }
        // Create call frame: save current param values
        CallFrame frame;
        frame.execStackDepth = m_execStack.size();
        frame.paramNames = func->paramNames;
        for (const QString &param : func->paramNames) {
            if (m_variables.contains(param))
                frame.savedVariables[param] = m_variables[param];
        }
        // Bind arguments to parameters
        for (int i = 0; i < func->paramNames.size(); ++i) {
            QString argVal = interpolateVariables(node->argValues[i]);
            setVariable(func->paramNames[i], argVal);
        }
        m_callStack.append(frame);
        m_execStack.append({&func->children, 0, 0, 0});
        scheduleNextStep();
        break;
    }

    case NodeType::Return: {
        if (m_callStack.isEmpty()) {
            emit executionError(node->line, "RETURN outside of function");
            stop();
            return;
        }
        // Pop exec frames down to call frame depth
        while (m_execStack.size() > m_callStack.last().execStackDepth) {
            m_execStack.removeLast();
        }
        returnFromFunction();
        scheduleNextStep();
        break;
    }

    case NodeType::FunctionDef:
        // Should never be executed (extracted from root), skip
        scheduleNextStep();
        break;

    case NodeType::Goto:
        gotoLabel(node->stringValue);
        scheduleNextStep();
        break;

    case NodeType::OnTimeout:
        m_onTimeoutLabel = node->stringValue;
        scheduleNextStep();
        break;

    case NodeType::OnError:
        m_onErrorLabel = node->stringValue;
        scheduleNextStep();
        break;

    case NodeType::Abort:
        if (!node->stringValue.isEmpty()) {
            QString msg = interpolateVariables(node->stringValue);
            emit executionError(node->line, msg);
        }
        stop();
        break;

    case NodeType::Log: {
        QString msg = interpolateVariables(node->stringValue);
        emit logMessage(msg);
        scheduleNextStep();
        break;
    }

    case NodeType::Pause:
        emit pauseRequested();
        // Don't schedule next step — resumeAfterPause() will do it
        break;

    case NodeType::Expect:
        startExpect(node);
        break;

    case NodeType::Extract: {
        updateBuiltinVariables();
        QString value;
        switch (node->extractType) {
        case ExtractType::FromPosition: {
            int len = node->stringValue.toInt();
            value = readScreenText(node->intValue - 1, node->intValue2 - 1, len);
            break;
        }
        case ExtractType::FieldAt:
            value = readFieldText(node->intValue - 1, node->intValue2 - 1);
            break;
        case ExtractType::CursorRow:
            if (m_screenWidget) {
                QPoint pos = m_screenWidget->screenBuffer()->cursorPosition();
                value = QString::number(pos.y() + 1); // 1-based
            }
            break;
        case ExtractType::CursorCol:
            if (m_screenWidget) {
                QPoint pos = m_screenWidget->screenBuffer()->cursorPosition();
                value = QString::number(pos.x() + 1); // 1-based
            }
            break;
        case ExtractType::LineAt: {
            int row = node->intValue - 1; // 1-based to 0-based
            auto *buf = screenBuffer();
            if (buf && row >= 0 && row < buf->rows()) {
                value = readScreenText(row, 0, buf->cols());
            }
            break;
        }
        }
        setVariable(node->varName, value);
        scheduleNextStep();
        break;
    }

    default:
        scheduleNextStep();
        break;
    }
}

// --- EXPECT implementation ---

void ScriptExecutor::startExpect(const std::shared_ptr<ASTNode> &node) {
    updateBuiltinVariables();

    // Check if condition is already met
    if (checkExpectCondition(node)) {
        setVariable("$EXPECT_RESULT", "OK");
        scheduleNextStep();
        return;
    }

    // Start waiting
    m_expectActive = true;
    m_expectNode = node;
    m_expectTimer.start(m_timeout);
}

bool ScriptExecutor::checkExpectCondition(const std::shared_ptr<ASTNode> &node) const {
    if (!m_screenWidget) return false;

    bool result = false;

    switch (node->expectType) {
    case ExpectType::TextAnywhere:
        result = screenContainsText(node->stringValue);
        break;
    case ExpectType::TextAtPos:
        result = screenContainsTextAt(node->stringValue, node->intValue - 1, node->intValue2 - 1);
        break;
    case ExpectType::TextAtRow:
        result = screenContainsTextAtRow(node->stringValue, node->intValue - 1);
        break;
    case ExpectType::CursorAtPos: {
        auto *buf = screenBuffer();
        if (!buf) break;
        QPoint pos = buf->cursorPosition();
        result = (pos.y() == node->intValue - 1 && pos.x() == node->intValue2 - 1);
        break;
    }
    case ExpectType::CursorAtRow: {
        auto *buf = screenBuffer();
        if (!buf) break;
        QPoint pos = buf->cursorPosition();
        result = (pos.y() == node->intValue - 1);
        break;
    }
    case ExpectType::KeyboardUnlocked:
        result = (m_screenWidget->keyboardState() == ui::widgets::KeyboardState::Unlocked);
        break;
    case ExpectType::KeyboardErrorLocked:
        result = (m_screenWidget->keyboardState() == ui::widgets::KeyboardState::ErrorLocked);
        break;
    case ExpectType::FieldContains: {
        QString fieldText = readFieldText(node->intValue - 1, node->intValue2 - 1);
        result = fieldText.contains(node->stringValue);
        break;
    }
    case ExpectType::MessageWaiting:
        result = m_screenWidget->messageWaiting();
        break;
    }

    if (node->negated) result = !result;
    return result;
}

void ScriptExecutor::endExpect(bool success) {
    m_expectTimer.stop();
    m_expectActive = false;
    m_expectNode = nullptr;

    if (success) {
        setVariable("$EXPECT_RESULT", "OK");
        scheduleNextStep();
    } else {
        setVariable("$EXPECT_RESULT", "TIMEOUT");
        if (!m_onTimeoutLabel.isEmpty()) {
            gotoLabel(m_onTimeoutLabel);
            scheduleNextStep();
        } else {
            scheduleNextStep();
        }
    }
}

// --- Condition evaluation ---

bool ScriptExecutor::evaluateCondition(const QString &left, CompareOp op, const QString &right) const {
    // CONTAINS is always a string operation
    if (op == CompareOp::Contains)
        return left.contains(right);

    // Try numeric comparison first
    bool leftIsNum = false, rightIsNum = false;
    int leftNum = left.toInt(&leftIsNum);
    int rightNum = right.toInt(&rightIsNum);

    if (leftIsNum && rightIsNum) {
        switch (op) {
        case CompareOp::Eq: return leftNum == rightNum;
        case CompareOp::Ne: return leftNum != rightNum;
        case CompareOp::Lt: return leftNum < rightNum;
        case CompareOp::Gt: return leftNum > rightNum;
        case CompareOp::Le: return leftNum <= rightNum;
        case CompareOp::Ge: return leftNum >= rightNum;
        case CompareOp::Contains: break; // handled above
        }
    }

    // String comparison
    int cmp = left.compare(right);
    switch (op) {
    case CompareOp::Eq: return cmp == 0;
    case CompareOp::Ne: return cmp != 0;
    case CompareOp::Lt: return cmp < 0;
    case CompareOp::Gt: return cmp > 0;
    case CompareOp::Le: return cmp <= 0;
    case CompareOp::Ge: return cmp >= 0;
    case CompareOp::Contains: break; // handled above
    }
    return false;
}

// --- Variable support ---

QString ScriptExecutor::resolveVariable(const QString &name) const {
    // name includes the $ prefix
    return m_variables.value(name, "0");
}

void ScriptExecutor::setVariable(const QString &name, const QString &value) {
    m_variables[name] = value;
}

QString ScriptExecutor::interpolateVariables(const QString &text) const {
    QString result;
    int i = 0;
    while (i < text.length()) {
        if (text[i] == '$') {
            if (i + 1 < text.length() && text[i + 1] == '$') {
                // Escaped $
                result += '$';
                i += 2;
            } else {
                // Read variable name
                int start = i;
                i++; // skip $
                while (i < text.length() && (text[i].isLetterOrNumber() || text[i] == '_')) i++;
                if (i - start <= 1) {
                    // Lone $ with no variable name — emit literal $
                    result += '$';
                } else {
                    QString varName = text.mid(start, i - start);
                    result += resolveVariable(varName);
                }
            }
        } else {
            result += text[i];
            i++;
        }
    }
    return result;
}

// --- GOTO ---

void ScriptExecutor::gotoLabel(const QString &label) {
    if (!m_callStack.isEmpty()) {
        emit executionError(0, "GOTO is not allowed inside functions");
        stop();
        return;
    }
    if (!m_parseResult.labels.contains(label)) {
        emit executionError(0, QString("Label '%1' not found").arg(label));
        stop();
        return;
    }

    // Clear execution stack and jump to label in root
    m_execStack.clear();
    int targetIndex = m_parseResult.labels[label] + 1; // +1 to skip the LABEL node itself
    m_execStack.append({&m_parseResult.root->children, targetIndex, 0, 0});
}

// --- Function return ---

void ScriptExecutor::returnFromFunction() {
    if (m_callStack.isEmpty()) return;

    CallFrame frame = m_callStack.last();
    m_callStack.removeLast();

    // Restore saved parameter variables
    for (const QString &param : frame.paramNames) {
        if (frame.savedVariables.contains(param)) {
            m_variables[param] = frame.savedVariables[param];
        } else {
            m_variables.remove(param);
        }
    }
}

// --- Built-in variables ---

void ScriptExecutor::updateBuiltinVariables() {
    if (!m_screenWidget) return;

    auto *buf = screenBuffer();
    if (buf) {
        m_variables["$SCREEN_ROWS"] = QString::number(buf->rows());
        m_variables["$SCREEN_COLS"] = QString::number(buf->cols());
        QPoint pos = buf->cursorPosition();
        m_variables["$CURSOR_ROW"] = QString::number(pos.y() + 1);
        m_variables["$CURSOR_COL"] = QString::number(pos.x() + 1);
    }

    switch (m_screenWidget->keyboardState()) {
    case ui::widgets::KeyboardState::Unlocked:
        m_variables["$KEYBOARD_STATE"] = "UNLOCKED";
        break;
    case ui::widgets::KeyboardState::Locked:
        m_variables["$KEYBOARD_STATE"] = "LOCKED";
        break;
    case ui::widgets::KeyboardState::ErrorLocked:
        m_variables["$KEYBOARD_STATE"] = "ERRORLOCKED";
        break;
    case ui::widgets::KeyboardState::SystemRequest:
        m_variables["$KEYBOARD_STATE"] = "SYSTEMREQUEST";
        break;
    }

    m_variables["$MESSAGE_WAITING"] = m_screenWidget->messageWaiting() ? "1" : "0";
}

// --- Screen text helpers ---

QString ScriptExecutor::readScreenText(int row, int col, int length) const {
    auto *buf = screenBuffer();
    if (!buf) return {};
    if (row < 0 || row >= buf->rows() || col < 0) return {};

    QString result;
    for (int i = 0; i < length && col + i < buf->cols(); ++i) {
        uint8_t ch = buf->character(row, col + i);
        result += core::EBCDIC::ebcdicToChar(ch);
    }
    return result;
}

QString ScriptExecutor::readFieldText(int row, int col) const {
    auto *buf = screenBuffer();
    if (!buf) return {};

    auto field = buf->getField(row, col);
    if (field.length == 0) return {};

    QByteArray data = buf->getFieldData(field);
    return core::EBCDIC::ebcdicToString(data);
}

bool ScriptExecutor::screenContainsText(const QString &text) const {
    auto *buf = screenBuffer();
    if (!buf) return false;

    for (int row = 0; row < buf->rows(); ++row) {
        QString rowText;
        for (int col = 0; col < buf->cols(); ++col) {
            rowText += core::EBCDIC::ebcdicToChar(buf->character(row, col));
        }
        if (rowText.contains(text)) return true;
    }
    return false;
}

bool ScriptExecutor::screenContainsTextAt(const QString &text, int row, int col) const {
    auto *buf = screenBuffer();
    if (!buf || row < 0 || row >= buf->rows() || col < 0) return false;

    for (int i = 0; i < text.length() && col + i < buf->cols(); ++i) {
        QChar screenChar = core::EBCDIC::ebcdicToChar(buf->character(row, col + i));
        if (screenChar != text[i]) return false;
    }
    return true;
}

bool ScriptExecutor::screenContainsTextAtRow(const QString &text, int row) const {
    auto *buf = screenBuffer();
    if (!buf || row < 0 || row >= buf->rows()) return false;

    QString rowText;
    for (int col = 0; col < buf->cols(); ++col) {
        rowText += core::EBCDIC::ebcdicToChar(buf->character(row, col));
    }
    return rowText.contains(text);
}

} // namespace core::scripting
