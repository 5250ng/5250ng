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

#include <QNetworkRequest>
#include <QObject>
#include <QString>

namespace agent {

enum class AuthType { ApiKey, OAuth };

class AuthMethod : public QObject {
    Q_OBJECT

  public:
    explicit AuthMethod(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~AuthMethod() = default;

    virtual AuthType type() const = 0;
    virtual bool isAuthenticated() const = 0;

    /// Apply authentication headers to an outgoing request.
    virtual void applyAuth(QNetworkRequest &request) = 0;

    /// Initiate authentication (no-op for API key; launches browser for OAuth).
    virtual void authenticate() = 0;

    /// Clear stored credentials.
    virtual void logout() = 0;

  signals:
    void authenticationSucceeded();
    void authenticationFailed(const QString &error);
    void tokenRefreshed();
};

} // namespace agent
