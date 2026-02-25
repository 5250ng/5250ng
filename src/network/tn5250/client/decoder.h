#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>

namespace tn5250::client {

// TN5250 command codes (based on RFC 1205 and IBM documentation)
enum class TN5250Command : uint8_t {
    WRITE_STRUCTURED_FIELD = 0x11,
    ERASE_WRITE = 0x05,
    ERASE_WRITE_ALTERNATE = 0x0D,
    READ_MODIFY = 0x06,
    READ_MODIFY_WRITE = 0x07,
    READ_MDT_FIELDS = 0x52
};

// TN5250 structured field types
enum class StructuredFieldType : uint8_t {
    OUTBOUND_3270DS = 0x00,
    SCS = 0x01,
    OUTBOUND_5250_DS = 0x02,
    INBOUND_3270DS = 0x80,
    INBOUND_5250_DS = 0x82
};

// Minimal parser state (reserved for future use)
enum class ParserState {
    WaitingForCommand,
    ReadingLength,
    ReadingData,
    Complete
};

// Application-layer TN5250 decoder (post-Telnet)
class Decoder : public QObject {
    Q_OBJECT

  public:
    explicit Decoder(QObject *parent = nullptr);

    // Parse incoming application data (post-Telnet layer)
    void parseData(const QByteArray &data);

    void reset();
    ParserState state() const { return m_state; }

  signals:
    void commandReceived(TN5250Command cmd, const QByteArray &data);
    void structuredFieldReceived(StructuredFieldType type, const QByteArray &data);
    void rawScreenDataReceived(const QByteArray &data);
    void clearScreenRequested();
    void keyboardUnlockRequested();
    void parseError(const QString &error);

  private:
    ParserState m_state;
    QByteArray m_buffer;
};

} // namespace tn5250::client
