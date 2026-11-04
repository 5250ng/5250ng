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

#include "terminal_theme.h"
#include <QMap>
#include <QMutex>
#include <QObject>

namespace ui::themes {

class TerminalThemeManager : public QObject {
    Q_OBJECT

  public:
    static TerminalThemeManager &instance();

    // Load themes from resources and user directory
    void loadBuiltinThemes();
    void loadUserThemes();

    // Theme registry
    void registerTheme(const TerminalTheme &theme);
    // Registers a theme from the theme editor and returns the id to use.
    // `changed` tells whether the user edited it. A built-in is never
    // replaced: an edited one becomes a saved user theme derived from it,
    // under a new id; an unchanged one is left as it is. A user theme keeps
    // its id and its identity (built-in flag, description, parent).
    QString applyEditedTheme(const TerminalTheme &edited, bool changed);
    QStringList availableThemes() const;
    bool hasTheme(const QString &id) const;
    TerminalTheme theme(const QString &id) const;
    TerminalTheme resolvedTheme(const QString &id) const;

    // User theme management
    bool saveUserTheme(const TerminalTheme &theme);
    bool deleteUserTheme(const QString &id);
    TerminalTheme duplicateTheme(const QString &id, const QString &newDisplayName);

    // Import/Export (.5250theme or .json)
    bool importTheme(const QString &filePath);
    bool exportTheme(const QString &id, const QString &filePath) const;
    bool exportTheme(const TerminalTheme &theme, const QString &filePath) const;

    // Default theme
    static QString defaultThemeId() { return QStringLiteral("classic_green"); }

  signals:
    void themeRegistered(const QString &id);
    void themeRemoved(const QString &id);

  private:
    explicit TerminalThemeManager(QObject *parent = nullptr);
    TerminalThemeManager(const TerminalThemeManager &) = delete;
    TerminalThemeManager &operator=(const TerminalThemeManager &) = delete;

    bool registerThemeFromJsonResource(const QString &resourcePath);
    QString userThemesDir() const;
    QString uniqueId(const QString &baseId) const;

    QMap<QString, TerminalTheme> m_themes;
    mutable QMutex m_mutex;
};

} // namespace ui::themes
