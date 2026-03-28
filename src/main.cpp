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

#include "agent/config.h"
#include "logger/logger.h"
#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"
#include "ui/themes/manager.h"
#include "ui/themes/font_loader.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Register uint8_t for queued signal/slot connections (used by sendGDS)
    qRegisterMetaType<uint8_t>("uint8_t");

    app.setApplicationName("5250ng");
    app.setApplicationVersion("0.5.0");
    app.setOrganizationName("5250ng");

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("5250ng - TN5250 Terminal Emulator");
    parser.addHelpOption();
    parser.addVersionOption();

    // Debug option
    QCommandLineOption debugOption(QStringList() << "d" << "debug", "Enable debug output");
    parser.addOption(debugOption);

    // Host option
    QCommandLineOption hostOption(QStringList() << "H" << "host", "Hostname or IP address to connect to", "hostname");
    parser.addOption(hostOption);

    // Port option
    QCommandLineOption portOption(QStringList() << "p" << "port", "Port number (default: 23)", "port", "23");
    parser.addOption(portOption);

    // TLS option
    QCommandLineOption tlsOption(QStringList() << "tls", "Use TLS/SSL encryption");
    parser.addOption(tlsOption);

    // Session option (load saved session by name)
    QCommandLineOption sessionOption(QStringList() << "s" << "load-session-from-name", "Load a saved session by name", "name");
    parser.addOption(sessionOption);

    // Session file option (load session from JSON file path)
    QCommandLineOption sessionFileOption(QStringList() << "f" << "load-session-from-file", "Load a session from a JSON file", "path");
    parser.addOption(sessionFileOption);

    // MCP server options
    QCommandLineOption mcpEnableOption(QStringList() << "enable-mcp-server", "Enable the MCP server on startup");
    parser.addOption(mcpEnableOption);
    QCommandLineOption mcpPortOption(QStringList() << "mcp-server-port", "MCP server port (default: 9250)", "port");
    parser.addOption(mcpPortOption);

    parser.process(app);

    // Set debug level if requested
    if (parser.isSet(debugOption)) {
        logger::Logger::instance()->setLogLevel(logger::LogLevel::Debug);
        logger::Logger::instance()->debug("Debug mode enabled");
    }

    // Create session config from command line options
    session::SessionConfig autoConnectConfig;
    bool autoConnect = false;

    int connectOptionCount = (parser.isSet(hostOption) ? 1 : 0)
                           + (parser.isSet(sessionOption) ? 1 : 0)
                           + (parser.isSet(sessionFileOption) ? 1 : 0);
    if (connectOptionCount > 1) {
        logger::Logger::instance()->error(
            "Options --host, --session, and --session-file are mutually exclusive"
        );
        return 1;
    }

    if (parser.isSet(sessionOption)) {
        QString sessionName = parser.value(sessionOption);
        session::SessionManager mgr;
        if (!mgr.loadSession(sessionName, autoConnectConfig)) {
            logger::Logger::instance()->error(
                QString("Session not found: %1").arg(sessionName)
            );
            return 1;
        }
        autoConnect = true;
        logger::Logger::instance()->debug(
            QString("Loading saved session: %1").arg(sessionName)
        );
    } else if (parser.isSet(sessionFileOption)) {
        QString filePath = parser.value(sessionFileOption);
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            logger::Logger::instance()->error(
                QString("Cannot open session file: %1").arg(filePath)
            );
            return 1;
        }
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isNull() || !doc.isObject()) {
            logger::Logger::instance()->error(
                QString("Invalid JSON in session file: %1").arg(filePath)
            );
            return 1;
        }
        if (!autoConnectConfig.fromJson(doc.object())) {
            logger::Logger::instance()->error(
                QString("Invalid session configuration in: %1").arg(filePath)
            );
            return 1;
        }
        autoConnect = true;
        logger::Logger::instance()->debug(
            QString("Loading session from file: %1").arg(filePath)
        );
    } else if (parser.isSet(hostOption)) {
        QString hostname = parser.value(hostOption);
        bool portOk = false;
        int portInt = parser.value(portOption).toInt(&portOk);

        if (!portOk || portInt < 1 || portInt > 65535) {
            logger::Logger::instance()->error(
                QString("Invalid port number: %1").arg(parser.value(portOption))
            );
            return 1;
        }

        quint16 port = static_cast<quint16>(portInt);
        bool useTLS = parser.isSet(tlsOption);

        autoConnectConfig.setName("Command Line Session");
        autoConnectConfig.setHostname(hostname);
        autoConnectConfig.setPort(port);
        autoConnectConfig.setUseTLS(useTLS);
        autoConnectConfig.setDeviceName("IBM-3179-2");
        autoConnectConfig.setScreenRows(24);
        autoConnectConfig.setScreenCols(80);

        if (autoConnectConfig.isValid()) {
            autoConnect = true;
            logger::Logger::instance()->debug(
                QString("Auto-connect configured: %1:%2%3")
                    .arg(hostname)
                    .arg(port)
                    .arg(useTLS ? " (TLS)" : "")
            );
        } else {
            logger::Logger::instance()->error(
                "Invalid connection parameters from command line"
            );
            return 1;
        }
    }

    // Apply MCP server CLI overrides to in-memory config (before MainWindow reads it).
    // These are session-only overrides — not persisted to disk, so the next launch
    // without the flag will respect the saved config (default: disabled).
    if (parser.isSet(mcpEnableOption)) {
        auto &agentCfg = agent::AgentConfig::instance();
        agentCfg.load();
        agentCfg.setMcpServerEnabled(true);
        if (parser.isSet(mcpPortOption)) {
            bool portOk = false;
            int mcpPort = parser.value(mcpPortOption).toInt(&portOk);
            if (portOk && mcpPort >= 1024 && mcpPort <= 65535) {
                agentCfg.setMcpServerPort(static_cast<quint16>(mcpPort));
            } else {
                logger::Logger::instance()->error(
                    QString("Invalid MCP port number: %1").arg(parser.value(mcpPortOption)));
                return 1;
            }
        }
        logger::Logger::instance()->debug(
            QString("MCP server enabled via CLI (port %1)").arg(agentCfg.mcpServerPort()));
    }

    // Register bundled terminal fonts before any widget creation
    ui::themes::loadBundledFonts();

    // Load themes early so UI can query theme colors
    ui::themes::ThemeManager::instance().loadBuiltinThemes();
    // Optionally set a default theme
    ui::themes::ThemeManager::instance().setCurrentTheme("dark");

    // Load terminal themes (builtin + user-created)
    ui::themes::TerminalThemeManager::instance().loadBuiltinThemes();
    ui::themes::TerminalThemeManager::instance().loadUserThemes();

    MainWindow win;
    win.show();

    // Auto-connect if host was provided
    if (autoConnect) {
        win.autoConnect(autoConnectConfig);
    }

    return app.exec();
}