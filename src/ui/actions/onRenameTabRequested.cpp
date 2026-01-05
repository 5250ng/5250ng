#include "../main_window.h"
#include <QInputDialog>
#include <QLineEdit>
#include <QString>

using namespace core;

void MainWindow::onRenameTabRequested(int index) {
    if (index < 0 || index >= m_sessions.size()) {
        return;
    }
    bool ok = false;
    QString current = m_tabWidget->tabText(index);
    QString name = QInputDialog::getText(
        this, "Rename Session", "New name:", QLineEdit::Normal, current, &ok
    );
    if (ok && !name.isEmpty()) {
        m_tabWidget->setTabText(index, name);
    }
}
