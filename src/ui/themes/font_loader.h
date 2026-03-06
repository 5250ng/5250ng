#pragma once

namespace ui::themes {

// Register bundled terminal fonts (IBM 3270, IBM Plex Mono) with QFontDatabase.
// Call once at application startup before creating any widgets.
void loadBundledFonts();

} // namespace ui::themes
