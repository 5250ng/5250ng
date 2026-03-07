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
