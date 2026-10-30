// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QJsonValue>
#include <QSet>
#include <QString>
#include <QtGlobal>
#include <cmath>

namespace mcp {

inline bool parseTcpPort(const QJsonValue &value, quint16 *port) {
    if (!port) return false;
    if (value.isUndefined()) {
        *port = 23;
        return true;
    }
    if (!value.isDouble()) return false;

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number
        || number < 1 || number > 65535) {
        return false;
    }
    *port = static_cast<quint16>(number);
    return true;
}

/// Error text for an MCP input tool whose first keystroke would be discarded
/// by Q5250ScreenWidget::keyPressEvent in the given keyboard state, or an
/// empty string when the widget accepts it. firstKey is the uppercased
/// press_key name, or empty for typed text. Arrow keys move the cursor
/// directly rather than through keyPressEvent, so they are never rejected.
inline QString lockedKeyboardError(ui::widgets::KeyboardState state,
                                   const QString &firstKey) {
    using ui::widgets::KeyboardState;
    static const QSet<QString> arrows = {"UP", "DOWN", "LEFT", "RIGHT"};
    if (arrows.contains(firstKey)) return QString();

    if (state == KeyboardState::Locked) {
        static const QSet<QString> allowed = {"ATTN", "SYSREQ"};
        if (allowed.contains(firstKey)) return QString();
        return QStringLiteral(
            "Keyboard is locked waiting for the host; the input was not sent. "
            "Wait for the screen to update (e.g. wait_for_text) and retry.");
    }
    if (state == KeyboardState::ErrorLocked) {
        static const QSet<QString> allowed = {"ESC", "ESCAPE", "ATTN", "HELP"};
        if (allowed.contains(firstKey)) return QString();
        return QStringLiteral(
            "Keyboard is error-locked; the input was not sent. "
            "Press ESC (Error Reset) first, then retry.");
    }
    return QString();
}

} // namespace mcp
