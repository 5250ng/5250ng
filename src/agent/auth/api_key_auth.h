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

#include "auth_method.h"

namespace agent {

/// Authentication style used for the API key header.
enum class ApiKeyStyle {
    XApiKey,       // x-api-key: <key>  (Anthropic)
    BearerToken,   // Authorization: Bearer <key>  (OpenAI)
};

class ApiKeyAuth : public AuthMethod {
    Q_OBJECT

  public:
    explicit ApiKeyAuth(ApiKeyStyle style, QObject *parent = nullptr);

    AuthType type() const override { return AuthType::ApiKey; }
    bool isAuthenticated() const override;
    void applyAuth(QNetworkRequest &request) override;
    void authenticate() override {}
    void logout() override;

    void setApiKey(const QString &key) { m_apiKey = key; }
    QString apiKey() const { return m_apiKey; }

  private:
    QString m_apiKey;
    ApiKeyStyle m_style;
};

} // namespace agent
