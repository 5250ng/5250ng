![](./.github/)

<p align="center">
  <!-- TODO: Replace owner/repo with actual GitHub path -->
  <a href="https://github.com/5250ng/5250ng/actions"><img src="https://img.shields.io/github/actions/workflow/status/5250ng/5250ng/build.yml?branch=main&style=flat-square" alt="Build"></a>
  <a href="https://github.com/5250ng/5250ng/releases"><img src="https://img.shields.io/github/v/release/5250ng/5250ng?style=flat-square" alt="Release"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square" alt="C++17">
  <img src="https://img.shields.io/badge/Qt-6-41cd52?style=flat-square&logo=qt&logoColor=white" alt="Qt6">
  <a href="LICENSE"><img src="https://img.shields.io/github/license/5250ng/5250ng?style=flat-square" alt="License"></a>
  <a href="https://twitter.com/podalirius_"><img src="https://img.shields.io/twitter/follow/podalirius_?style=flat-square&logo=twitter" alt="Twitter"></a>
</p>

---

**5250ng** is a full-featured 5250ng terminal emulator built with **Qt6** and **C++17**. It implements the complete 5250ng protocol ([RFC 1205](https://datatracker.ietf.org/doc/html/rfc1205)) for connecting to IBM AS/400 and IBM i systems, with a modern UI, multi-session tabs, macro recording, file transfer, and custom terminal themes.

<img src="./.github/i5_OS_Login.png" width=48%> <img src="./.github/i5_OS_Main_Menu.png" width=48%>

## Features

- [x] **Protocol:** Full 5250ng (RFC 1205), telnet option negotiation, TLS/SSL encryption
- [x] **Display:** Custom QPainter rendering, 24×80 and 27×132 modes, CRT scanline overlay
- [x] **Input:** PF1-PF24, field navigation, keyboard remapping
- [x] **Sessions:** Multi-tab, persistent profiles (JSON), auto-connect, per-session settings
- [x] **Macros:** Record/playback with timing, hotkey assignment, import/export
- [x] **File Transfer:** IBM i IFS client - browse, upload, download, mkdir, rename, delete
- [x] **Themes:** 13 built-in + user-defined themes with live preview
- [x] **Code Pages:** CP037, CP273, CP277, CP278, CP280, CP284, CP285, CP297, CP500
- [x] **Extras:** Hotspot detection, screen history/scrollback, regex match & replace, session logging
- [x] **Compatibility**: 5250ng works on **Linux**, **macOS**, and **Windows**. It requires Qt 6 and a C++17 compiler.

## Installation

### Linux (Ubuntu / Debian)

```bash
sudo apt install qt6-base-dev cmake g++ libxkbcommon-dev libssl-dev
cmake -S . -B build
cmake --build build
./build/bin/5250ng
```

### Mac OS

```bash
brew install qt cmake openssl
cmake -S . -B build
cmake --build build
./build/bin/5250ng.app/Contents/MacOS/5250ng
```

### Windows (PowerShell)

```powershell
cmake -G "Visual Studio 17 2022" -S . -B build
cmake --build build --config Release
.\build\bin\Release\5250ng.exe
```

> **TLS support** requires Qt6 SSL libraries. On Debian/Ubuntu: `sudo apt install libqt6network6 libssl-dev` then rebuild.

## Usage

```
5250ng [options]

Options:
  -H, --host <hostname>   Host to connect to (auto-connects at startup)
  -p, --port <port>       Port number (default: 23)
  --tls                   Use TLS/SSL encryption
  -d, --debug             Enable debug output
  -h, --help              Show help
  -v, --version           Show version
```

```bash
# Auto-connect to a server
./build/bin/5250ng --host as400.example.com

# Connect with TLS on port 992
./build/bin/5250ng --host as400.example.com --port 992 --tls

# Launch the GUI and use the connect dialog
./build/bin/5250ng
```

## Feature Showcase

<details>
<summary><strong>Themes</strong></summary>

5250ng ships with 13 terminal themes. Switch themes instantly from the menu or per-session settings.

| Theme | Style |
|-------|-------|
| `amber_phosphor` | Classic amber CRT |
| `blue_terminal` | Blue-on-black retro |
| `classic_green` | IBM 3270-style green screen |
| `classic_white` | White terminal |
| `dracula` | Dark with purple/red accents |
| `high_contrast` | Accessibility-focused |
| `ibm_3179` | Faithful IBM 3179 color display |
| `ibm_infowindow_ii` | IBM InfoWindow II emulation |
| `matrix` | Green Matrix style |
| `modern_dark` | Contemporary dark |
| `monokai` | Developer-friendly dark |
| `nord` | Arctic north-bluish palette |
| `solarized_dark` | Solarized dark scheme |

You can also create your own themes by adding JSON files to the themes directory.

<!-- TODO: Screenshot grid showing 4 themes side by side (e.g. classic_green, dracula,
     ibm_3179, solarized_dark), each connected to the same screen. Save as
     .github/screenshots/themes.png -->

</details>

<details>
<summary><strong>Macro Recording & Playback</strong></summary>

Record keystroke sequences with timing, assign hotkeys, and replay them. Macros are saved as JSON and can be imported/exported between installations.

<!-- TODO: Screenshot of the macro management dialog showing a recorded macro with
     keystrokes and timing. Save as .github/screenshots/macros.png -->

</details>

<details>
<summary><strong>IFS File Transfer</strong></summary>

Browse, upload, and download files from the IBM i Integrated File System (IFS). Supports directory listing, file operations, and progress tracking.

<!-- TODO: Screenshot of the file transfer dialog showing an IFS directory listing with
     files and folders. Save as .github/screenshots/file_transfer.png -->

</details>

<details>
<summary><strong>Multi-Session Tabs</strong></summary>

Open multiple connections in tabs. Each session has its own settings, theme, and connection profile. Sessions can be duplicated, renamed, and persist across restarts.

<!-- TODO: Screenshot showing 3-4 open tabs with different session names and themes.
     Save as .github/screenshots/tabs.png -->

</details>

<details>
<summary><strong>Hotspot Detection</strong></summary>

Automatically detects clickable elements on screen - function key labels (F3=Exit), menu options, and URLs - and makes them clickable.

<!-- TODO: Screenshot of a screen with highlighted hotspots (e.g. F3=Exit, F12=Cancel
     shown as clickable). Save as .github/screenshots/hotspots.png -->

</details>

## Contributing

Pull requests and issues are welcome. Please ensure your changes pass the existing test suite before submitting.

## License

This project is licensed under the [GNU General Public License v2.0](LICENSE).

## Credits

Developed by [@p0dalirius](https://github.com/p0dalirius).
