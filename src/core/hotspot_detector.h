#pragma once

#include <QColor>
#include <QRect>
#include <QString>
#include <QVector>
#include <cstdint>

namespace core {

struct Hotspot {
    int row;
    int startCol;
    int endCol;
    QString label;
    uint8_t aidByte;     // AID code to send (e.g., F3 = 0x33)
    QString menuNumber;  // For menu items like "1. User tasks"

    enum Type { FunctionKey, MenuItem };
    Type type;
};

class HotspotDetector {
  public:
    HotspotDetector() = default;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    // Scan screen buffer text for hotspot patterns
    // screenText: row-major array of Unicode chars (rows x cols)
    QVector<Hotspot> detect(const QVector<QChar> &screenText, int rows, int cols) const;

    // Find hotspot at a given cell position
    static const Hotspot *hotspotAt(const QVector<Hotspot> &hotspots, int row, int col);

  private:
    bool m_enabled = false;

    void detectFunctionKeys(const QString &line, int row, QVector<Hotspot> &out) const;
    void detectMenuItems(const QString &line, int row, QVector<Hotspot> &out) const;

    static uint8_t fkeyToAID(int fkeyNum);
};

} // namespace core
