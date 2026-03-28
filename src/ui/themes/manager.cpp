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

#include "manager.h"
#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>

namespace ui {
namespace themes {

// Forward declarations
static QString generateWidgetStylesheet(const QMap<QString, QString> &colors);

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
 * Normally not used directly by callers - use ThemeManager::instance()
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
    t.stylesheet = generateWidgetStylesheet(t.colors);
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

struct ThemeColors {
    QColor bg, windowText, base, altBase, button, mid, highlight, disabledText;
    bool isDark;
};

static ThemeColors deriveThemeColors(const QMap<QString, QString> &colors) {
    ThemeColors c;
    c.bg           = QColor(colors.value("mainwindow.background", "#1e1e1e"));
    c.isDark       = c.bg.lightness() < 128;
    c.windowText   = colors.contains("mainwindow.text")
                         ? QColor(colors.value("mainwindow.text"))
                         : (c.isDark ? QColor("#d4d4d4") : QColor("#212121"));
    c.base         = c.isDark ? QColor("#252526") : QColor("#ffffff");
    c.altBase      = c.isDark ? QColor("#2d2d30") : QColor("#f0f0f0");
    c.button       = c.isDark ? QColor("#3c3c3c") : QColor("#e0e0e0");
    c.mid          = QColor(colors.value("mainwindow.titlebar.hline",
                            c.isDark ? "#3c3c3c" : "#cccccc"));
    c.highlight    = QColor("#0065ff");
    c.disabledText = c.isDark ? QColor("#6d6d6d") : QColor("#9e9e9e");
    return c;
}

static QString generateWidgetStylesheet(const QMap<QString, QString> &colors) {
    const ThemeColors c = deriveThemeColors(colors);
    const QString bg           = c.bg.name();
    const QString text         = c.windowText.name();
    const QString base         = c.base.name();
    const QString altBase      = c.altBase.name();
    const QString button       = c.button.name();
    const QString buttonLighter = c.button.lighter(110).name();
    const QString mid          = c.mid.name();
    const QString highlight    = c.highlight.name();
    const QString highlightDark = c.highlight.darker(120).name();
    const QString disabledText = c.disabledText.name();

    return QStringLiteral(
        /* ── QComboBox ── */
        "QComboBox {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 3px;"
        "  padding: 4px 8px;"
        "  min-height: 20px;"
        "}"
        "QComboBox:hover { border-color: %4; }"
        "QComboBox:focus { border-color: %4; }"
        "QComboBox:disabled {"
        "  color: %5;"
        "  background-color: %6;"
        "}"
        "QComboBox:on {"
        "  border-bottom-left-radius: 0;"
        "  border-bottom-right-radius: 0;"
        "}"
        "QComboBox::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 20px;"
        "  border-left: 1px solid %3;"
        "  border-top-right-radius: 3px;"
        "  border-bottom-right-radius: 3px;"
        "  background-color: %7;"
        "}"
        "QComboBox::drop-down:hover { background-color: %8; }"
        "QComboBox::down-arrow {"
        "  width: 0; height: 0;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 5px solid %2;"
        "}"
        "QComboBox::down-arrow:disabled { border-top-color: %5; }"
        "QComboBox QAbstractItemView {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  selection-background-color: %4;"
        "  selection-color: #ffffff;"
        "  outline: none;"
        "  padding: 2px;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  padding: 4px 8px;"
        "  min-height: 20px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background-color: %4;"
        "  color: #ffffff;"
        "}"

        /* ── QMenuBar ── */
        "QMenuBar {"
        "  background-color: transparent;"
        "  color: %2;"
        "  border: none;"
        "  spacing: 2px;"
        "}"
        "QMenuBar::item {"
        "  background-color: transparent;"
        "  color: %2;"
        "  padding: 4px 8px;"
        "  border-radius: 3px;"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: %4;"
        "  color: #ffffff;"
        "}"
        "QMenuBar::item:pressed {"
        "  background-color: %9;"
        "}"

        /* ── QMenu ── */
        "QMenu {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  padding: 4px 0px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 28px 6px 24px;"
        "  background-color: transparent;"
        "}"
        "QMenu::item:selected {"
        "  background-color: %4;"
        "  color: #ffffff;"
        "}"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator {"
        "  height: 1px;"
        "  background-color: %3;"
        "  margin: 4px 8px;"
        "}"
        "QMenu::indicator {"
        "  width: 14px; height: 14px;"
        "  margin-left: 6px;"
        "}"
        "QMenu::indicator:checked {"
        "  image: none;"
        "  background-color: %4;"
        "  border: 1px solid %4;"
        "  border-radius: 2px;"
        "}"
        "QMenu::indicator:unchecked {"
        "  background-color: transparent;"
        "  border: 1px solid %3;"
        "  border-radius: 2px;"
        "}"
        "QMenu::right-arrow {"
        "  width: 0; height: 0;"
        "  border-top: 4px solid transparent;"
        "  border-bottom: 4px solid transparent;"
        "  border-left: 5px solid %2;"
        "  margin-right: 8px;"
        "}"
    ).arg(base,       // %1
          text,       // %2
          mid,        // %3
          highlight,  // %4
          disabledText, // %5
          altBase,    // %6
          button,     // %7
          buttonLighter, // %8
          highlightDark  // %9
    );
}

static QPalette buildPaletteFromTheme(const QMap<QString, QString> &colors) {
    const ThemeColors c = deriveThemeColors(colors);

    QPalette pal;
    for (const auto group : {QPalette::Active, QPalette::Inactive}) {
        pal.setColor(group, QPalette::Window,          c.bg);
        pal.setColor(group, QPalette::WindowText,      c.windowText);
        pal.setColor(group, QPalette::Base,            c.base);
        pal.setColor(group, QPalette::AlternateBase,   c.altBase);
        pal.setColor(group, QPalette::Text,            c.windowText);
        pal.setColor(group, QPalette::Button,          c.button);
        pal.setColor(group, QPalette::ButtonText,      c.windowText);
        pal.setColor(group, QPalette::Highlight,       c.highlight);
        pal.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
        pal.setColor(group, QPalette::ToolTipBase,     c.altBase);
        pal.setColor(group, QPalette::ToolTipText,     c.windowText);
        pal.setColor(group, QPalette::Mid,             c.mid);
        pal.setColor(group, QPalette::Dark,            c.bg.darker(150));
        pal.setColor(group, QPalette::Midlight,        c.button.lighter(110));
        pal.setColor(group, QPalette::Light,           c.button.lighter(160));
        pal.setColor(group, QPalette::Shadow,          c.bg.darker(200));
        pal.setColor(group, QPalette::Link,            QColor("#3794ff"));
        pal.setColor(group, QPalette::LinkVisited,     QColor("#7f3fbf"));
        pal.setColor(group, QPalette::PlaceholderText, c.disabledText);
    }
    pal.setColor(QPalette::Disabled, QPalette::Window,       c.bg);
    pal.setColor(QPalette::Disabled, QPalette::Base,         c.base);
    pal.setColor(QPalette::Disabled, QPalette::AlternateBase, c.altBase);
    pal.setColor(QPalette::Disabled, QPalette::Button,       c.button);
    pal.setColor(QPalette::Disabled, QPalette::WindowText,   c.disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Text,         c.disabledText);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText,   c.disabledText);
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
