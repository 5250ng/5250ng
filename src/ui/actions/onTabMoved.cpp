#include "../main_window.h"
#include <QTabWidget>
#include <QtGlobal>

using namespace core;

void MainWindow::onTabMoved(int from, int to) {
    if (from < 0 || from >= m_sessions.size() || to < 0 ||
        to >= m_sessions.size()) {
        return;
    }
    m_sessions.move(from, to);
    if (m_activeIndex == from) {
        m_activeIndex = to;
    } else if (m_activeIndex >= qMin(from, to) &&
               m_activeIndex <= qMax(from, to)) {
        // Active index may shift by one depending on move direction
        // Simplest is to refresh active session pointers
        setActiveSession(m_tabWidget->currentIndex());
    }
}
