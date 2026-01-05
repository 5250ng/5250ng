

# 5250ng

A modern TN5250 terminal emulator built with Qt6 and C++, featuring a layered modular architecture.

## Usage

### Command Line Options

```bash
tn5250 [options]
```

**Options:**
- `-d, --debug` - Enable debug output
- `-H, --host <hostname>` - Hostname or IP address to connect to (auto-connects at startup)
- `-p, --port <port>` - Port number (default: 23)
- `--tls` - Use TLS/SSL encryption
- `-h, --help` - Display help message
- `-v, --version` - Display version information

**Examples:**
```bash
# Connect to a server with auto-connect
./build/bin/tn5250 --host example.com --port 23

# Connect with TLS
./build/bin/tn5250 --host example.com --port 992 --tls

# Enable debug output
./build/bin/tn5250 --debug --host example.com

# Run without auto-connect (use GUI connect dialog)
./build/bin/tn5250
```

## Project Structure

The project follows a layered architecture with clear separation of concerns:

- **Core** (`src/core/`) - Foundation components: protocol parser, EBCDIC conversion, screen buffer, configuration
- **Transport** (`src/transport/`) - Network communication: socket client, TN5250 handshake, TLS support
- **Display** (`src/display/`) - Rendering engine: custom QWidget, QPainter-based rendering, attributes
- **UI** (`src/ui/`) - User interface: main window, dialogs, session management
- **AI** (`src/ai/`) - Optional AI integration: provider abstraction, insights (Phase 6)

## Build Instructions

### Linux

**Basic installation (without TLS support):**
```bash
sudo apt install qt6-base-dev cmake g++ xkb-data libxkbcommon-dev libxkbfile-dev 
cmake -S . -B build
cmake --build build
./build/bin/tn5250
```

**With TLS/SSL support:**
```bash
sudo apt install qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools libqt6core6 libqt6network6 libssl-dev 
# Note: Qt6 SSL support may require additional packages depending on your distribution
cmake -S . -B build
cmake --build build
./build/bin/tn5250
```

**Note:** If you see "TLS not supported - Qt6 SSL not found" error, you need to install Qt6 SSL libraries. On Ubuntu/Debian, try:
```bash
sudo apt install libqt6network6 libssl-dev
# Then rebuild the project
cmake -S . -B build
cmake --build build
```

**Note:** If you see "Could NOT find XKB" error, you need to install XKB development libraries. On Ubuntu/Debian, try:
```bash
sudo apt install libxkbcommon-dev
# Then rebuild the project
cmake -S . -B build
cmake --build build
```

### macOS

```bash
brew install qt cmake
cmake -S . -B build
cmake --build build
./build/bin/tn5250.app/Contents/MacOS/tn5250
```

### Windows (PowerShell)

```bash
cmake -G "Visual Studio 17 2022" -S . -B build
cmake --build build --config Release
```

The executable will be in `build/bin/` directory.
