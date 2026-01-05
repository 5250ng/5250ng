#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace tn5250::devices {

struct Device {
    QString model;
    int lines;
    int columns;
    bool supportsColors;
    bool supportsDbcs;
};

// Return immutable list of supported devices
const QVector<Device> &supportedDevices();

// Find a device by exact model name; returns nullptr if not found
const Device *findSupportedDevice(const QString &model);

} // namespace tn5250::devices
