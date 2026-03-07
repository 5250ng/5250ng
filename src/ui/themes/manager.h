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

#include "themes.h"
#include <QApplication>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPalette>
#include <QString>

namespace ui {
namespace themes {

class ThemeManager : public QObject {
    Q_OBJECT

  public:
    static ThemeManager &instance();

    void registerTheme(const Theme &theme);
    bool registerThemeFromJsonResource(const QString &resourcePath);
    bool registerThemeFromJson(const QString &name, const QByteArray &jsonBytes);
    QStringList availableThemes() const;
    bool hasTheme(const QString &name) const;
    Theme theme(const QString &name) const;

    // Select and apply a theme by name. Returns false if theme not found.
    bool setCurrentTheme(const QString &name);
    QString currentThemeName() const;
    Theme currentTheme() const;

    // Apply the current theme to the running application
    void applyToApplication(QApplication *app = qApp);

    // Load the built-in themes provided by our data files
    void loadBuiltinThemes();

    // Resolve a color string (e.g., "#0065ff") from the current theme by key.
    // Returns defaultValue if the key is not found or theme not set.
    QString color(const QString &key, const QString &defaultValue = QString()) const;

  signals:
    void themeChanged(const ui::themes::Theme &theme);

  private:
    explicit ThemeManager(QObject *parent = nullptr);
    ThemeManager(const ThemeManager &) = delete;
    ThemeManager &operator=(const ThemeManager &) = delete;

    QMap<QString, Theme> m_themes;
    QString m_current;
    mutable QMutex m_mutex;
};

} // namespace themes
} // namespace ui
