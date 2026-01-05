#pragma once

#include "logger/logger.h"
#include "session/worker.h"
#include "ui/widgets/Frameless/BaseFramelessWindow.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

class LogViewerDialog : public ui::widgets::BaseFramelessWindow {
    Q_OBJECT
  public:
    explicit LogViewerDialog(QWidget *parent = nullptr);
    explicit LogViewerDialog(tn5250::session::Worker *worker, QWidget *parent = nullptr);
    ~LogViewerDialog() override = default;

  private slots:
    void onLogMessage(logger::LogLevel level, const QString &message);
    void onSessionLogAppended(const QString &line);
    void onFindToggle();
    void onFindReturnPressed();
    void onFindNext();
    void onFindPrev();

  private:
    QWidget *m_findBar;
    QLineEdit *m_findEdit;
    QPushButton *m_nextBtn;
    QPushButton *m_prevBtn;
    QShortcut *m_shortcutFind;
    QPlainTextEdit *m_text;
    void loadExisting();
    tn5250::session::Worker *m_worker = nullptr;
};
