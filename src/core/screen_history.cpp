#include "screen_history.h"

namespace core {

ScreenHistory::ScreenHistory(int maxDepth) : m_maxDepth(maxDepth) {
    m_history.reserve(maxDepth);
}

void ScreenHistory::setMaxDepth(int depth) {
    m_maxDepth = depth;
    while (m_history.size() > m_maxDepth)
        m_history.removeLast();
}

void ScreenHistory::push(const ScreenSnapshot &snapshot) {
    m_history.prepend(snapshot);
    if (m_history.size() > m_maxDepth)
        m_history.removeLast();
}

void ScreenHistory::clear() {
    m_history.clear();
}

const ScreenSnapshot &ScreenHistory::at(int index) const {
    return m_history.at(index);
}

QVector<int> ScreenHistory::search(const QString &text) const {
    QVector<int> results;
    QByteArray searchBytes = text.toUtf8();
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].cellData.contains(searchBytes))
            results.append(i);
    }
    return results;
}

} // namespace core
