#include "manager.h"
#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>

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
    const bool changed = (m_current != name);
    m_current = name;
    Theme th = m_themes.value(name);
    locker.unlock();
    // Always (re-)apply so the palette is set even on first call at startup.
    applyToApplication(qApp);
    if (changed) {
        emit themeChanged(th);
    }
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
 * Build a complete QPalette from a theme's colors map.
 *
 * Derives foreground/background palette roles from the two key color entries
 * ("mainwindow.background" and "mainwindow.titlebar.hline").  Detects
 * dark vs. light by the background luminance and fills all standard roles
 * so that Qt widgets render correctly without needing an explicit stylesheet.
 */
static QPalette buildPaletteFromTheme(const QMap<QString, QString> &colors) {
    const QColor bg(colors.value("mainwindow.background", "#1e1e1e"));
    const bool isDark = bg.lightness() < 128;

    const QColor windowText   = isDark ? QColor("#d4d4d4") : QColor("#212121");
    const QColor base         = isDark ? QColor("#252526") : QColor("#ffffff");
    const QColor altBase      = isDark ? QColor("#2d2d30") : QColor("#f0f0f0");
    const QColor button       = isDark ? QColor("#3c3c3c") : QColor("#e0e0e0");
    const QColor mid          = QColor(colors.value("mainwindow.titlebar.hline",
                                      isDark ? "#3c3c3c" : "#cccccc"));
    const QColor highlight    = QColor("#0065ff");
    const QColor disabledText = isDark ? QColor("#6d6d6d") : QColor("#9e9e9e");

    QPalette pal;
    for (const auto group : {QPalette::Active, QPalette::Inactive}) {
        pal.setColor(group, QPalette::Window,          bg);
        pal.setColor(group, QPalette::WindowText,      windowText);
        pal.setColor(group, QPalette::Base,            base);
        pal.setColor(group, QPalette::AlternateBase,   altBase);
        pal.setColor(group, QPalette::Text,            windowText);
        pal.setColor(group, QPalette::Button,          button);
        pal.setColor(group, QPalette::ButtonText,      windowText);
        pal.setColor(group, QPalette::Highlight,       highlight);
        pal.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
        pal.setColor(group, QPalette::ToolTipBase,     altBase);
        pal.setColor(group, QPalette::ToolTipText,     windowText);
        pal.setColor(group, QPalette::Mid,             mid);
        pal.setColor(group, QPalette::Dark,            bg.darker(150));
        pal.setColor(group, QPalette::Midlight,        button.lighter(110));
        pal.setColor(group, QPalette::Light,           button.lighter(160));
        pal.setColor(group, QPalette::Shadow,          bg.darker(200));
        pal.setColor(group, QPalette::Link,            QColor("#3794ff"));
        pal.setColor(group, QPalette::LinkVisited,     QColor("#7f3fbf"));
        pal.setColor(group, QPalette::PlaceholderText, disabledText);
    }
    pal.setColor(QPalette::Disabled, QPalette::WindowText,  disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Text,        disabledText);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText,  disabledText);
    return pal;
}

/**
 * Apply the active theme's palette and stylesheet to the given application.
 *
 * Builds a complete QPalette from the theme's named colors so that all Qt
 * widgets automatically inherit the correct dark/light appearance.  If the
 * theme also carries an explicit stylesheet it is applied on top.
 *
 * @param app QApplication pointer (defaults to qApp if not provided elsewhere).
 */
void ThemeManager::applyToApplication(QApplication *app) {
    if (!app) {
        return;
    }
    const Theme th = currentTheme();
    app->setPalette(buildPaletteFromTheme(th.colors));
    app->setStyleSheet(th.stylesheet);
}

/**
 * Register the built-in themes (Dark, Light, High Contrast).
 *
 * Invokes builder functions in data/ to construct and register
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
