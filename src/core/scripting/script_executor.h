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

#include "script_ast.h"
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <functional>

namespace ui::widgets {
class ScreenBuffer;
class Q5250ScreenWidget;
} // namespace ui::widgets

namespace core::scripting {

class ScriptExecutor : public QObject {
    Q_OBJECT

  public:
    explicit ScriptExecutor(QObject *parent = nullptr);

    void setScreenWidget(ui::widgets::Q5250ScreenWidget *widget);

    void execute(const ParseResult &parseResult);
    void stop();
    bool isRunning() const { return m_running; }
    void setInitialVariables(const QHash<QString, QString> &vars);

  signals:
    // Key injection — connect these to MainWindow lambdas that synthesize QKeyEvents
    void injectKeyPress(int key, Qt::KeyboardModifiers mods, const QString &text);
    void injectAIDKey(uint8_t aidByte);

    // Cursor movement
    void moveCursor(int row, int col);
    void moveCursorStep(const QString &direction); // UP/DOWN/LEFT/RIGHT
    void gotoInputField(int fieldIndex); // >0: 1-based index, -1: NEXT, -2: PREVIOUS

    // Status
    void executionStarted();
    void executionFinished();
    void executionError(int line, const QString &message);
    void logMessage(const QString &message);
    void pauseRequested();

  public slots:
    void resumeAfterPause();
    void notifyScreenChanged();
    void notifyTerminalStateChanged();

  private slots:
    void executeStep();

  private:
    // Execution state
    bool m_running = false;
    ParseResult m_parseResult;
    int m_timeout = 30000;       // EXPECT timeout in ms
    int m_actionDelay = 0;       // Global fixed delay in ms (GLOBAL DELAY)
    int m_jitterMin = 0;         // Global random delay min in ms (GLOBAL JITTER)
    int m_jitterMax = 0;         // Global random delay max in ms (GLOBAL JITTER)

    // Function call stack
    struct CallFrame {
        QHash<QString, QString> savedVariables;  // pre-call values of parameters
        QVector<QString> paramNames;             // to know which to restore
        int execStackDepth;                      // m_execStack.size() at call time
    };
    QMap<QString, std::shared_ptr<ASTNode>> m_functions;
    QVector<CallFrame> m_callStack;
    void returnFromFunction();

    // Execution stack for nested blocks
    struct ExecFrame {
        QVector<std::shared_ptr<ASTNode>> *nodes;
        int index;
        int repeatCount;        // For REPEAT blocks
        int currentRepeat;
    };
    QVector<ExecFrame> m_execStack;

    // Variables
    QHash<QString, QString> m_variables;
    QHash<QString, QString> m_initialVariables;
    QString resolveVariable(const QString &name) const;
    void setVariable(const QString &name, const QString &value);
    QString interpolateVariables(const QString &text) const;

    // Current node execution
    void executeNode(const std::shared_ptr<ASTNode> &node);
    void scheduleNextStep(int delayMs = 0);

    // EXPECT implementation
    void startExpect(const std::shared_ptr<ASTNode> &node);
    bool checkExpectCondition(const std::shared_ptr<ASTNode> &node) const;
    void endExpect(bool success);
    bool m_expectActive = false;
    std::shared_ptr<ASTNode> m_expectNode;
    QTimer m_expectTimer;

    // Condition evaluation
    bool evaluateCondition(const QString &left, CompareOp op, const QString &right) const;

    // GOTO support (only at root level)
    void gotoLabel(const QString &label);

    // Error handlers
    QString m_onTimeoutLabel;
    QString m_onErrorLabel;

    // AID key unlock waiting
    bool m_waitingForUnlock = false;

    // Screen widget reference
    ui::widgets::Q5250ScreenWidget *m_screenWidget = nullptr;
    ui::widgets::ScreenBuffer *screenBuffer() const;

    // Built-in variable updates
    void updateBuiltinVariables();

    // Screen text helpers
    QString readScreenText(int row, int col, int length) const;
    QString readFieldText(int row, int col) const;
    bool screenContainsText(const QString &text) const;
    bool screenContainsTextAt(const QString &text, int row, int col) const;
    bool screenContainsTextAtRow(const QString &text, int row) const;
};

} // namespace core::scripting
