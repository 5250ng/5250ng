#include "../main_window.h"

void MainWindow::onDataReceived(const QByteArray &data) {
    // Parse incoming data
    logger::Logger::instance()->debug(
        QString("MainWindow: onDataReceived - %1 bytes").arg(data.size())
    );

    // Check if this looks like TN5250 commands
    if (data.size() > 0) {
        uint8_t firstByte = static_cast<uint8_t>(data[0]);
        bool isTN5250Command = firstByte == 0x05 || firstByte == 0x06 ||
                               firstByte == 0x07 || firstByte == 0x0D ||
                               firstByte == 0x11 || firstByte == 0xA0;

        if (isTN5250Command) {
            logger::Logger::instance()->debug(
                QString("MainWindow: Data starts with TN5250 command byte 0x%1")
                    .arg(firstByte, 2, 16, QChar('0'))
            );
        } else {
            logger::Logger::instance()->warning(
                QString(
                    "MainWindow: Data starts with non-TN5250 byte 0x%1 - "
                    "expected TN5250 command (0x05, 0x06, 0x07, 0x0D, 0x11, 0xA0)"
                )
                    .arg(firstByte, 2, 16, QChar('0'))
            );
        }
    }

    if (m_parser) {
        m_parser->parseData(data);
    } else {
        logger::Logger::instance()->warning("MainWindow: Parser is null!");
    }
}