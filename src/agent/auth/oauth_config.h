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

#pragma once

#include <QString>

namespace agent {

/// Configuration for an OAuth2 Authorization Code + PKCE flow.
struct OAuthConfig {
    QString authorizationEndpoint;  // e.g. "https://auth.example.com/authorize"
    QString tokenEndpoint;          // e.g. "https://auth.example.com/token"
    QString clientId;               // Registered application client_id
    QString scope;                  // Space-separated scopes
    QString providerName;           // Display name for UI
};

} // namespace agent
