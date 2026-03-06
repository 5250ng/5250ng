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
