#pragma once

#include <QApplication>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPalette>
#include <QString>

namespace ui {
namespace themes {

struct Theme {
    QString name;        // unique id, e.g., "dark", "light"
    QString displayName; // user-facing, e.g., "Dark"
    QPalette palette;    // full app palette
    QString stylesheet;  // optional global stylesheet
    QMap<QString, QString> colors; // arbitrary key -> color string ("#rrggbb" or named)
};

} // namespace themes
} // namespace ui
