// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <QJsonValue>
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

} // namespace mcp
