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

#include "api_key_auth.h"

namespace agent {

ApiKeyAuth::ApiKeyAuth(ApiKeyStyle style, QObject *parent)
    : AuthMethod(parent), m_style(style) {}

bool ApiKeyAuth::isAuthenticated() const {
    return !m_apiKey.isEmpty();
}

void ApiKeyAuth::applyAuth(QNetworkRequest &request) {
    if (m_apiKey.isEmpty()) return;

    switch (m_style) {
    case ApiKeyStyle::XApiKey:
        request.setRawHeader("x-api-key", m_apiKey.toUtf8());
        break;
    case ApiKeyStyle::BearerToken:
        request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
        break;
    }
}

void ApiKeyAuth::logout() {
    m_apiKey.clear();
}

} // namespace agent
