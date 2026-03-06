#include "font_loader.h"
#include "logger/logger.h"
#include <QFontDatabase>

namespace ui::themes {

void loadBundledFonts() {
    static const char *fontResources[] = {
        ":/fonts/data/fonts/3270-Regular.ttf",
        ":/fonts/data/fonts/3270SemiCondensed-Regular.ttf",
        ":/fonts/data/fonts/IBMPlexMono-Regular.ttf",
        ":/fonts/data/fonts/IBMPlexMono-Bold.ttf",
    };

    for (const char *res : fontResources) {
        int id = QFontDatabase::addApplicationFont(res);
        if (id < 0) {
            logger::Logger::instance()->warning(
                QString("Failed to load bundled font: %1").arg(res));
        } else {
            QStringList families = QFontDatabase::applicationFontFamilies(id);
            logger::Logger::instance()->debug(
                QString("Loaded bundled font: %1 -> %2")
                    .arg(res, families.join(", ")));
        }
    }
}

} // namespace ui::themes
