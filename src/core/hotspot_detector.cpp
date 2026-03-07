#include "hotspot_detector.h"
#include <QRegularExpression>

namespace core {

QVector<Hotspot> HotspotDetector::detect(const QVector<QChar> &screenText, int rows, int cols) const {
    QVector<Hotspot> hotspots;
    if (!m_enabled || screenText.size() < rows * cols)
        return hotspots;

    for (int r = 0; r < rows; ++r) {
        QString line;
        line.reserve(cols);
        for (int c = 0; c < cols; ++c)
            line.append(screenText[r * cols + c]);
        detectFunctionKeys(line, r, hotspots);
        detectMenuItems(line, r, hotspots);
    }
    return hotspots;
}

void HotspotDetector::detectFunctionKeys(const QString &line, int row, QVector<Hotspot> &out) const {
    // Match patterns like "F3=Exit", "F12=Cancel", "F1=Help"
    static const QRegularExpression rx(R"(F(\d{1,2})\s*[=:]\s*(\S[^\s]*(?:\s\S+)*?)\b)",
                                       QRegularExpression::CaseInsensitiveOption);

    auto it = rx.globalMatch(line);
    while (it.hasNext()) {
        auto m = it.next();
        int fkey = m.captured(1).toInt();
        if (fkey < 1 || fkey > 24) continue;

        Hotspot h;
        h.row = row;
        h.startCol = m.capturedStart(0);
        h.endCol = m.capturedEnd(0) - 1;
        h.label = m.captured(0).trimmed();
        h.aidByte = fkeyToAID(fkey);
        h.type = Hotspot::FunctionKey;
        out.append(h);
    }
}

void HotspotDetector::detectMenuItems(const QString &line, int row, QVector<Hotspot> &out) const {
    // Match patterns like "1. User tasks", "12. Work with jobs"
    // Only match at start of significant content (after leading spaces)
    static const QRegularExpression rx(R"(^\s{2,}(\d{1,2})\.\s+(\S.+?)\s*$)");

    auto m = rx.match(line);
    if (m.hasMatch()) {
        Hotspot h;
        h.row = row;
        h.startCol = m.capturedStart(1);
        h.endCol = m.capturedEnd(2) - 1;
        h.label = m.captured(0).trimmed();
        h.menuNumber = m.captured(1);
        h.aidByte = 0; // Menu items type the number + Enter
        h.type = Hotspot::MenuItem;
        out.append(h);
    }
}

const Hotspot *HotspotDetector::hotspotAt(const QVector<Hotspot> &hotspots, int row, int col) {
    for (const auto &h : hotspots) {
        if (h.row == row && col >= h.startCol && col <= h.endCol)
            return &h;
    }
    return nullptr;
}

uint8_t HotspotDetector::fkeyToAID(int fkeyNum) {
    // 5250 AID bytes for F1-F24 (per IBM SA21-9247)
    static const uint8_t aids[] = {
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, // F1-F8
        0x39, 0x3A, 0x3B, 0x3C,                           // F9-F12
        0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, // F13-F20
        0xB9, 0xBA, 0xBB, 0xBC                            // F21-F24
    };
    if (fkeyNum >= 1 && fkeyNum <= 24)
        return aids[fkeyNum - 1];
    return 0;
}

} // namespace core
