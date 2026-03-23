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

#include "token_storage.h"
#include <QSettings>

namespace agent {

TokenStorage &TokenStorage::instance() {
    static TokenStorage s;
    return s;
}

void TokenStorage::saveTokens(const QString &providerId, const QString &accessToken,
                              const QString &refreshToken, const QDateTime &expiresAt) {
    QSettings settings;
    settings.beginGroup("AI/OAuth/" + providerId);
    settings.setValue("accessToken", accessToken);
    settings.setValue("refreshToken", refreshToken);
    settings.setValue("expiresAt", expiresAt.toString(Qt::ISODate));
    settings.endGroup();
}

bool TokenStorage::loadTokens(const QString &providerId, QString &accessToken,
                              QString &refreshToken, QDateTime &expiresAt) const {
    QSettings settings;
    settings.beginGroup("AI/OAuth/" + providerId);
    accessToken = settings.value("accessToken").toString();
    refreshToken = settings.value("refreshToken").toString();
    QString expiresStr = settings.value("expiresAt").toString();
    settings.endGroup();

    if (accessToken.isEmpty()) return false;

    expiresAt = QDateTime::fromString(expiresStr, Qt::ISODate);
    return true;
}

void TokenStorage::clearTokens(const QString &providerId) {
    QSettings settings;
    settings.beginGroup("AI/OAuth/" + providerId);
    settings.remove("");
    settings.endGroup();
}

} // namespace agent
