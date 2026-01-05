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
