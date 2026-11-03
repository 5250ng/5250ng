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

#include "version.h"
#include "agent/config.h"
#include "core/keyboard_mapping.h"
#include "logger/logger.h"
#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"
#include "ui/themes/manager.h"
#include "ui/themes/app_theme_setting.h"
#include "ui/themes/font_loader.h"
#include "ui/themes/terminal_theme_manager.h"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// On Windows GUI apps (WinMain), stdout/stderr are not connected to any
// console.  If launched from cmd.exe or PowerShell, attach to the parent
// console so qDebug/qInfo output is visible.  Otherwise allocate a new one.
static void attachWindowsConsole() {
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE *f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
    }
#endif
}

// Reports a command-line error on stderr and returns the exit status. The
// message is printed as is (Logger output goes through qCritical, which
// quotes it, and is hidden unless -d is given), and on Windows a console is
// attached first when stderr is not already redirected, since a GUI app has
// none.
static int commandLineError(const QString &message) {
#ifdef Q_OS_WIN
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err == nullptr || err == INVALID_HANDLE_VALUE)
        attachWindowsConsole();
#endif
    std::fprintf(stderr, "5250ng: %s\n", qPrintable(message));
    std::fflush(stderr);
    return 1;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Force Fusion style for cross-platform visual consistency
    app.setStyle(QStyleFactory::create("Fusion"));

    // Register uint8_t for queued signal/slot connections (used by sendGDS)
    qRegisterMetaType<uint8_t>("uint8_t");

    app.setApplicationName("5250ng");
    app.setApplicationDisplayName("5250ng");
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName("5250ng");
    app.setWindowIcon(QIcon(":/icons/5250ng.png"));

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
    QCommandLineOption portOption(QStringList() << "p" << "port", "Port number (default: 23, or 992 with --tls)", "port");
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

    // PCAP replay option (replay a captured session instead of connecting)
    QCommandLineOption replayPcapOption(QStringList() << "replay-pcap", "Replay a TN5250 session from a pcap/pcapng capture file", "path");
    parser.addOption(replayPcapOption);

    // MCP server options
    QCommandLineOption mcpEnableOption(QStringList() << "enable-mcp-server", "Enable the MCP server on startup");
    parser.addOption(mcpEnableOption);
    QCommandLineOption mcpPortOption(QStringList() << "mcp-server-port", "MCP server port (default: 9250)", "port");
    parser.addOption(mcpPortOption);

    parser.process(app);

    // Set debug level if requested on the command line or turned on from
    // Help > Enable Debug Logging in a previous run
    if (parser.isSet(debugOption)) {
        attachWindowsConsole();
    }
    if (parser.isSet(debugOption)
        || QSettings().value(logger::kDebugLoggingSettingKey, false).toBool()) {
        logger::Logger::instance()->setDebugLogging(true);
    }

    // Options that only make sense together with another one are an error
    // on their own, rather than being silently ignored.
    if (!parser.positionalArguments().isEmpty()) {
        return commandLineError(
            QString("Unexpected argument: %1").arg(parser.positionalArguments().constFirst()));
    }
    if (!parser.isSet(hostOption)) {
        if (parser.isSet(portOption))
            return commandLineError("--port requires --host");
        if (parser.isSet(tlsOption))
            return commandLineError("--tls requires --host");
    }
    if (parser.isSet(mcpPortOption) && !parser.isSet(mcpEnableOption))
        return commandLineError("--mcp-server-port requires --enable-mcp-server");

    // Create session config from command line options
    session::SessionConfig autoConnectConfig;
    bool autoConnect = false;

    int connectOptionCount = (parser.isSet(hostOption) ? 1 : 0)
                           + (parser.isSet(sessionOption) ? 1 : 0)
                           + (parser.isSet(sessionFileOption) ? 1 : 0)
                           + (parser.isSet(replayPcapOption) ? 1 : 0);
    if (connectOptionCount > 1) {
        return commandLineError(
            "Options --host, --load-session-from-name, --load-session-from-file, "
            "and --replay-pcap are mutually exclusive");
    }

    QString replayPcapPath;
    if (parser.isSet(replayPcapOption)) {
        replayPcapPath = parser.value(replayPcapOption);
        if (!QFile::exists(replayPcapPath)) {
            return commandLineError(QString("Capture file not found: %1").arg(replayPcapPath));
        }
    }

    if (parser.isSet(sessionOption)) {
        QString sessionName = parser.value(sessionOption);
        session::SessionManager mgr;
        if (!mgr.loadSession(sessionName, autoConnectConfig)) {
            return commandLineError(QString("Session not found: %1").arg(sessionName));
        }
        autoConnect = true;
        logger::Logger::instance()->debug(
            QString("Loading saved session: %1").arg(sessionName)
        );
    } else if (parser.isSet(sessionFileOption)) {
        QString filePath = parser.value(sessionFileOption);
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return commandLineError(QString("Cannot open session file: %1").arg(filePath));
        }
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isNull() || !doc.isObject()) {
            return commandLineError(QString("Invalid JSON in session file: %1").arg(filePath));
        }
        if (!autoConnectConfig.fromJson(doc.object())) {
            return commandLineError(QString("Invalid session configuration in: %1").arg(filePath));
        }
        autoConnect = true;
        logger::Logger::instance()->debug(
            QString("Loading session from file: %1").arg(filePath)
        );
    } else if (parser.isSet(hostOption)) {
        QString hostname = parser.value(hostOption);
        bool useTLS = parser.isSet(tlsOption);
        bool portOk = true;
        int portInt = useTLS ? 992 : 23;
        if (parser.isSet(portOption))
            portInt = parser.value(portOption).toInt(&portOk);

        if (!portOk || portInt < 1 || portInt > 65535) {
            return commandLineError(QString("Invalid port number: %1").arg(parser.value(portOption)));
        }

        quint16 port = static_cast<quint16>(portInt);

        autoConnectConfig.setName("Command Line Session");
        autoConnectConfig.setHostname(hostname);
        autoConnectConfig.setPort(port);
        autoConnectConfig.setUseTLS(useTLS);
        autoConnectConfig.setDeviceType("IBM-3179-2");
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
            return commandLineError("Invalid connection parameters from command line");
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
                agentCfg.pinMcpServerPort(static_cast<quint16>(mcpPort));
            } else {
                return commandLineError(
                    QString("Invalid MCP port number: %1").arg(parser.value(mcpPortOption)));
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
    // A stored theme that no longer exists falls back to the default
    if (!ui::themes::ThemeManager::instance().setCurrentTheme(ui::themes::savedAppTheme()))
        ui::themes::ThemeManager::instance().setCurrentTheme(QStringLiteral("dark"));

    // Load terminal themes (builtin + user-created)
    ui::themes::TerminalThemeManager::instance().loadBuiltinThemes();
    ui::themes::TerminalThemeManager::instance().loadUserThemes();

    // Load user keyboard mapping (falls back to defaults when none saved).
    core::KeyboardMapping::instance().load();

    MainWindow win;
    win.show();

    // Auto-connect if host was provided
    if (autoConnect) {
        win.autoConnect(autoConnectConfig);
    } else if (!replayPcapPath.isEmpty()) {
        // Open the replay tab once the event loop is running, mirroring
        // the autoConnect() startup deferral.
        QTimer::singleShot(100, &win, [&win, replayPcapPath]() {
            win.startPcapReplay(replayPcapPath);
        });
    }

    return app.exec();
}
