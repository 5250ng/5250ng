#include "manager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace ui {
namespace themes {

/**
 * Retrieve the global ThemeManager singleton.
 *
 * Creates the instance on first call and returns the same instance on
 * subsequent calls. This central manager stores available themes and
 * notifies listeners when the active theme changes.
 *
 * @return Reference to the global ThemeManager instance.
 */
ThemeManager &ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

/**
 * Construct a ThemeManager.
 *
 * Normally not used directly by callers — use ThemeManager::instance()
 * instead. The manager starts with an empty theme registry; call
 * loadBuiltinThemes() to register the default themes.
 *
 * @param parent Optional QObject parent.
 */
ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {}

/**
 * Register a theme in the manager.
 *
 * If this is the first theme being registered, it becomes the current theme.
 * Registration is thread-safe and replaces any existing theme with the
 * same name.
 *
 * @param theme Theme descriptor to register (name, displayName, palette, qss).
 */
void ThemeManager::registerTheme(const Theme &theme) {
    QMutexLocker locker(&m_mutex);
    m_themes.insert(theme.name, theme);
    if (m_current.isEmpty()) {
        m_current = theme.name;
    }
}

bool ThemeManager::registerThemeFromJsonResource(const QString &resourcePath) {
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();
    // If the resource file is named like ":/themes/dark.json" and the JSON has no name,
    // derive the internal name from filename.
    QString derivedName;
    const int slash = resourcePath.lastIndexOf('/');
    if (slash >= 0) {
        derivedName = resourcePath.mid(slash + 1);
        if (derivedName.endsWith(".json")) {
            derivedName.chop(5);
        }
    }
    return registerThemeFromJson(derivedName, data);
}

bool ThemeManager::registerThemeFromJson(const QString &defaultName, const QByteArray &jsonBytes) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &err);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }
    const QJsonObject obj = doc.object();
    Theme t;
    t.name = obj.value("name").toString(defaultName);
    t.displayName = obj.value("displayName").toString(t.name);
    // Optional stylesheet/palette can be omitted in JSON-first approach
    // Colors map
    const QJsonValue colorsVal = obj.value("colors");
    if (colorsVal.isObject()) {
        const QJsonObject cobj = colorsVal.toObject();
        for (auto it = cobj.begin(); it != cobj.end(); ++it) {
            if (it.value().isString()) {
                t.colors.insert(it.key(), it.value().toString());
            }
        }
    }
    registerTheme(t);
    return true;
}

/**
 * Get a sorted list of theme names currently available.
 *
 * @return Case-insensitively sorted list of theme ids (internal names).
 */
QStringList ThemeManager::availableThemes() const {
    QMutexLocker locker(&m_mutex);
    QStringList names = m_themes.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

/**
 * Check whether a theme with the given name exists.
 *
 * @param name Internal theme id to look up.
 * @return True if the theme is registered, false otherwise.
 */
bool ThemeManager::hasTheme(const QString &name) const {
    QMutexLocker locker(&m_mutex);
    return m_themes.contains(name);
}

/**
 * Retrieve a theme by name.
 *
 * Returns a default-constructed Theme if the name is not known.
 *
 * @param name Internal theme id to fetch.
 * @return Theme descriptor for the given name, or empty Theme if missing.
 */
Theme ThemeManager::theme(const QString &name) const {
    QMutexLocker locker(&m_mutex);
    return m_themes.value(name, Theme{});
}

/**
 * Set and apply the current theme by name.
 *
 * If the theme exists and is not already active, updates the current theme,
 * applies its palette and stylesheet to the application, and emits
 * themeChanged(). Returns false if the theme name is unknown.
 *
 * @param name Internal theme id to activate.
 * @return True on success (or if already current), false if theme not found.
 */
bool ThemeManager::setCurrentTheme(const QString &name) {
    QMutexLocker locker(&m_mutex);
    if (!m_themes.contains(name)) {
        return false;
    }
    if (m_current == name) {
        return true;
    }
    m_current = name;
    Theme th = m_themes.value(name);
    locker.unlock();
    applyToApplication(qApp);
    emit themeChanged(th);
    return true;
}

/**
 * Get the internal name of the currently active theme.
 *
 * @return Current theme id, or empty if none is set.
 */
QString ThemeManager::currentThemeName() const {
    QMutexLocker locker(&m_mutex);
    return m_current;
}

/**
 * Get the currently active theme descriptor.
 *
 * @return Active Theme; may be default-constructed if none is set.
 */
Theme ThemeManager::currentTheme() const {
    QMutexLocker locker(&m_mutex);
    return m_themes.value(m_current, Theme{});
}

/**
 * Apply the active theme's palette and stylesheet to the given application.
 *
 * If the theme has a non-empty stylesheet, it is set on the application;
 * otherwise, any existing application stylesheet is cleared. The application
 * palette is also updated to the theme's palette.
 *
 * @param app QApplication pointer (defaults to qApp if not provided elsewhere).
 */
void ThemeManager::applyToApplication(QApplication *app) {
    if (!app) {
        return;
    }
    Theme th = currentTheme();
    if (!th.stylesheet.isEmpty()) {
        app->setStyleSheet(th.stylesheet);
    } else {
        app->setStyleSheet(QString());
    }
    app->setPalette(th.palette);
}

/**
 * Register the built-in themes (Dark, Light, High Contrast).
 *
 * Invokes builder functions in data/*.cpp to construct and register
 * the default theme set.
 */
void ThemeManager::loadBuiltinThemes() {
    // Load JSON resources embedded via themes.qrc
    registerThemeFromJsonResource(":/themes/data/dark.json");
    registerThemeFromJsonResource(":/themes/data/light.json");
    registerThemeFromJsonResource(":/themes/data/highcontrast.json");
}

QString ThemeManager::color(const QString &key, const QString &defaultValue) const {
    QMutexLocker locker(&m_mutex);
    const Theme th = m_themes.value(m_current, Theme{});
    if (th.colors.contains(key)) {
        return th.colors.value(key);
    }
    return defaultValue;
}

} // namespace themes
} // namespace ui
