#include "../main_window.h"
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

void MainWindow::onToggleSessionLogging() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->sessionLogger) {
        s->sessionLogger = new core::SessionLogger(s->container);
    }

    if (s->sessionLogger->isActive()) {
        s->sessionLogger->stop();
        m_sessionLoggingAction->setChecked(false);
        ui::widgets::StyledMessageBox::information(this, "Session Logging",
            QString("Logging stopped.\nLog saved to: %1").arg(s->sessionLogger->filePath()));
    } else {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + "/logs";
        QDir().mkpath(dir);
        QString filename = QString("session_%1_%2.log")
            .arg(s->config.hostname().replace('.', '_'))
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        QString path = dir + "/" + filename;

        if (s->sessionLogger->start(path)) {
            m_sessionLoggingAction->setChecked(true);
            s->sessionLogger->logEvent(
                QString("Connected to %1:%2").arg(s->config.hostname()).arg(s->config.port()));
        } else {
            ui::widgets::StyledMessageBox::warning(this, "Session Logging",
                QString("Failed to open log file:\n%1").arg(path));
        }
    }
}
