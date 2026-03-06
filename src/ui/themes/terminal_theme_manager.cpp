#include "terminal_theme_manager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

namespace ui::themes {

TerminalThemeManager &TerminalThemeManager::instance() {
    static TerminalThemeManager inst;
    return inst;
}

TerminalThemeManager::TerminalThemeManager(QObject *parent) : QObject(parent) {}

// --- Loading ---

void TerminalThemeManager::loadBuiltinThemes() {
    registerThemeFromJsonResource(":/themes/data/terminal/classic_green.json");
    registerThemeFromJsonResource(":/themes/data/terminal/classic_white.json");
    registerThemeFromJsonResource(":/themes/data/terminal/modern_dark.json");
    registerThemeFromJsonResource(":/themes/data/terminal/ibm_3179.json");
    registerThemeFromJsonResource(":/themes/data/terminal/high_contrast.json");
    registerThemeFromJsonResource(":/themes/data/terminal/solarized_dark.json");
}

bool TerminalThemeManager::registerThemeFromJsonResource(const QString &resourcePath) {
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    TerminalTheme theme = TerminalTheme::fromJson(doc.object());
    if (theme.id.isEmpty()) {
        // Derive id from filename
        const int slash = resourcePath.lastIndexOf('/');
        if (slash >= 0) {
            theme.id = resourcePath.mid(slash + 1);
            if (theme.id.endsWith(".json")) {
                theme.id.chop(5);
            }
        }
    }
    theme.builtin = true;
    registerTheme(theme);
    return true;
}

void TerminalThemeManager::loadUserThemes() {
    QDir dir(userThemesDir());
    if (!dir.exists()) {
        return;
    }

    const QStringList files = dir.entryList({"*.json", "*.5250theme"}, QDir::Files);
    for (const QString &file : files) {
        QFile f(dir.absoluteFilePath(file));
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();

        if (doc.isNull() || !doc.isObject()) {
            continue;
        }

        TerminalTheme theme = TerminalTheme::fromJson(doc.object());
        if (theme.id.isEmpty()) {
            continue;
        }
        theme.builtin = false;
        registerTheme(theme);
    }
}

// --- Registry ---

void TerminalThemeManager::registerTheme(const TerminalTheme &theme) {
    QMutexLocker locker(&m_mutex);
    m_themes.insert(theme.id, theme);
    locker.unlock();
    emit themeRegistered(theme.id);
}

QStringList TerminalThemeManager::availableThemes() const {
    QMutexLocker locker(&m_mutex);
    QStringList ids = m_themes.keys();
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

bool TerminalThemeManager::hasTheme(const QString &id) const {
    QMutexLocker locker(&m_mutex);
    return m_themes.contains(id);
}

TerminalTheme TerminalThemeManager::theme(const QString &id) const {
    QMutexLocker locker(&m_mutex);
    return m_themes.value(id, TerminalTheme{});
}

TerminalTheme TerminalThemeManager::resolvedTheme(const QString &id) const {
    QMutexLocker locker(&m_mutex);
    if (!m_themes.contains(id)) {
        return TerminalTheme{};
    }
    TerminalTheme t = m_themes.value(id);
    // Capture a copy of the themes map for the lookup lambda
    QMap<QString, TerminalTheme> themes = m_themes;
    locker.unlock();

    return t.resolved([&themes](const QString &lookupId) -> TerminalTheme {
        return themes.value(lookupId, TerminalTheme{});
    });
}

// --- User theme management ---

QString TerminalThemeManager::userThemesDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/terminal_themes";
}

QString TerminalThemeManager::uniqueId(const QString &baseId) const {
    QMutexLocker locker(&m_mutex);
    if (!m_themes.contains(baseId)) {
        return baseId;
    }
    for (int i = 1; i < 1000; ++i) {
        QString candidate = baseId + "_" + QString::number(i);
        if (!m_themes.contains(candidate)) {
            return candidate;
        }
    }
    return baseId + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8);
}

bool TerminalThemeManager::saveUserTheme(const TerminalTheme &theme) {
    QDir dir(userThemesDir());
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    QString filename = theme.id;
    filename.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    QString path = dir.absoluteFilePath(filename + ".json");

    QJsonDocument doc(theme.toJson());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();

    // Update registry
    TerminalTheme saved = theme;
    saved.builtin = false;
    registerTheme(saved);
    return true;
}

bool TerminalThemeManager::deleteUserTheme(const QString &id) {
    QMutexLocker locker(&m_mutex);
    if (!m_themes.contains(id)) {
        return false;
    }
    if (m_themes.value(id).builtin) {
        return false; // Cannot delete builtin themes
    }
    m_themes.remove(id);
    locker.unlock();

    // Remove file
    QDir dir(userThemesDir());
    QString filename = id;
    filename.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    dir.remove(filename + ".json");

    emit themeRemoved(id);
    return true;
}

TerminalTheme TerminalThemeManager::duplicateTheme(const QString &id,
                                                    const QString &newDisplayName) {
    TerminalTheme original = resolvedTheme(id);
    if (original.id.isEmpty()) {
        return TerminalTheme{};
    }

    // Generate a sanitized base id from the display name
    QString baseId = newDisplayName.toLower();
    baseId.replace(QRegularExpression("[^a-zA-Z0-9]"), "_");
    baseId.replace(QRegularExpression("_+"), "_");
    if (baseId.startsWith('_')) baseId = baseId.mid(1);
    if (baseId.endsWith('_'))   baseId.chop(1);
    if (baseId.isEmpty()) baseId = "custom_theme";

    TerminalTheme copy = original;
    copy.id          = uniqueId(baseId);
    copy.displayName = newDisplayName;
    copy.builtin     = false;
    copy.parentThemeId.clear(); // Resolved copy, no parent needed

    return copy;
}

// --- Import/Export ---

bool TerminalThemeManager::importTheme(const QString &filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    TerminalTheme theme = TerminalTheme::fromJson(doc.object());
    if (theme.id.isEmpty()) {
        return false;
    }

    // Ensure unique id if collision
    theme.id = uniqueId(theme.id);
    theme.builtin = false;

    // If the theme has embedded image data, extract it
    if (!theme.backgroundImageData.isEmpty()) {
        QDir dir(userThemesDir() + "/images");
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QString imgPath = dir.absoluteFilePath(theme.id + ".png");
        QFile imgFile(imgPath);
        if (imgFile.open(QIODevice::WriteOnly)) {
            imgFile.write(theme.backgroundImageData);
            imgFile.close();
            theme.backgroundImagePath = imgPath;
        }
    }

    return saveUserTheme(theme);
}

bool TerminalThemeManager::exportTheme(const QString &id, const QString &filePath) const {
    TerminalTheme t = resolvedTheme(id);
    if (t.id.isEmpty()) {
        return false;
    }

    // Embed background image if it exists
    if (t.backgroundMode == TerminalTheme::Image && !t.backgroundImagePath.isEmpty()
        && t.backgroundImageData.isEmpty()) {
        QFile imgFile(t.backgroundImagePath);
        if (imgFile.open(QIODevice::ReadOnly)) {
            t.backgroundImageData = imgFile.readAll();
            imgFile.close();
        }
    }

    QJsonDocument doc(t.toJson());
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

} // namespace ui::themes
