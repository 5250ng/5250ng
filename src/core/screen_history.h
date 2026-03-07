#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QVector>
#include <cstdint>

namespace core {

struct ScreenSnapshot {
    QDateTime timestamp;
    QByteArray cellData;   // Serialized ScreenCell array (raw bytes)
    int rows = 0;
    int cols = 0;
    int cursorRow = 0;
    int cursorCol = 0;
};

class ScreenHistory {
  public:
    explicit ScreenHistory(int maxDepth = 100);

    void setMaxDepth(int depth);
    int maxDepth() const { return m_maxDepth; }

    void push(const ScreenSnapshot &snapshot);
    void clear();

    int count() const { return m_history.size(); }
    bool isEmpty() const { return m_history.isEmpty(); }

    // Index 0 = most recent, count()-1 = oldest
    const ScreenSnapshot &at(int index) const;

    // Search screen text content
    QVector<int> search(const QString &text) const;

  private:
    int m_maxDepth;
    QVector<ScreenSnapshot> m_history;
};

} // namespace core
