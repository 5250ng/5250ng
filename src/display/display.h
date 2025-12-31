#pragma once

// Display layer - Rendering engine
// This layer contains:
// - Custom QWidget for TN5250 display (TN5250Widget)
// - QPainter-based rendering
// - Attribute handling (color, reverse video, blink, underline)
// - Cursor rendering
// - 27×132 mode support
// - Screen buffer (ScreenBuffer)

#include "screen_buffer.h"
#include "tn5250_widget.h"

namespace display {
    // All display components are now implemented
}

