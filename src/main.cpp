#include "logger/logger.h"
#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"
#include "ui/themes/manager.h"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

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

    parser.process(app);

    // Set debug level if requested
    if (parser.isSet(debugOption)) {
        logger::Logger::instance()->setLogLevel(logger::LogLevel::Debug);
        logger::Logger::instance()->debug("Debug mode enabled");
    }

    // Create session config from command line if host is provided
    session::SessionConfig autoConnectConfig;
    bool autoConnect = false;

    if (parser.isSet(hostOption)) {
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

    // Load themes early so UI can query theme colors
    ui::themes::ThemeManager::instance().loadBuiltinThemes();
    // Optionally set a default theme
    ui::themes::ThemeManager::instance().setCurrentTheme("dark");

    MainWindow win;
    win.show();

    // Auto-connect if host was provided
    if (autoConnect) {
        win.autoConnect(autoConnectConfig);
    }

    return app.exec();
}