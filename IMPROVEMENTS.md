# Proposed Improvements for 5250ng

## Current State

**5250ng** is a well-architected IBM TN5250 terminal emulator (v0.5.0) built with Qt6/C++17. Phases 1-5 are complete:

- **Full TN5250 protocol** (RFC 1205) with Telnet negotiation, GDS message parsing, and TLS/SSL
- **Display engine** — QPainter-based rendering with 16 colors, field management, cursor, CRT effects
- **Keyboard input** — F1-F24, AID codes, field navigation, EBCDIC encoding
- **9 EBCDIC code pages** (CP037, CP273, CP277, CP278, CP280, CP284, CP285, CP297, CP500)
- **Multi-session** tabs with persistent JSON config
- **Theming** — builtin + user themes, background images, CRT scanline effects
- **Custom frameless window**, connect dialog, settings, log viewer
- **7 test suites** covering protocol, encoding, screen buffer, and integration

---

## Tier 1 — High-Value, Practical

### 1. Macro Recording & Playback

Record keystroke sequences and replay them. Essential for repetitive AS/400 workflows (login sequences, menu navigation, data entry). Could support named macros with keyboard shortcuts.

**Key features:**
- Record/stop toggle via toolbar button or keyboard shortcut
- Named macro library with assignable hotkeys
- Macro editor for manual editing of recorded sequences
- Support for delays/waits (e.g., wait for specific screen content before continuing)
- Import/export macros as files for sharing between users

### 2. Printer Session Support (3812/SCS)

Implement a virtual printer device so host print jobs can be captured to PDF/file. Many AS/400 workflows rely on printer output.

**Key features:**
- Virtual printer device registration with the host
- SCS (SNA Character Stream) data stream parsing
- Output to PDF, plain text, or raw SCS files
- Print queue management UI
- Auto-open or auto-save options for incoming print jobs

### 3. File Transfer (Data Transfer)

Host-to-PC and PC-to-host file transfer using the 5250 data transfer facility. Common need for extracting reports or uploading data.

**Key features:**
- Download files/members from AS/400 libraries
- Upload local files to AS/400
- Progress indicator and transfer status
- Support for common formats (CSV, fixed-width, EBCDIC raw)
- Transfer history log

### 4. Hotspots / Auto-Detection

Detect clickable items on screen (menu numbers, function key labels like "F3=Exit") and make them mouse-clickable. Major UX win.

**Key features:**
- Detect patterns like `F3=Exit`, `F12=Cancel`, `1. User tasks`
- Underline or highlight detected hotspots on hover
- Click triggers the corresponding key or menu selection
- Configurable detection patterns (regex-based)
- Toggle hotspots on/off per user preference

---

## Tier 2 — UX & Quality of Life

### 5. Session Scripting (Lua/JS)

Expose a scripting API for automation: connect, wait for screen, send keys, read screen content. Useful for batch operations and testing.

**Key features:**
- Embedded scripting engine (Lua or JavaScript via Qt QJSEngine)
- API: `connect()`, `waitForScreen(pattern)`, `sendKeys(keys)`, `readScreen(row, col, len)`
- Script editor with syntax highlighting
- Run scripts from command line (headless mode)
- Script library with examples

### 6. Screen History / Scrollback

Keep a history of previous screens so users can scroll back to review past output without re-navigating on the host.

**Key features:**
- Configurable history depth (number of screens to retain)
- Scroll through history with keyboard shortcuts or scrollbar
- Visual indicator when viewing history vs. live screen
- Search within history
- Export history to file

### 7. Keyboard Remapping UI

Visual editor to remap physical keys to 5250 functions. Different users have strong preferences (e.g., mapping Escape to Attn, Ctrl+Enter to Field Exit).

**Key features:**
- Visual keyboard layout display
- Drag-and-drop or click-to-assign remapping
- Preset layouts (IBM default, PC-friendly, custom)
- Per-session or global keymaps
- Import/export keymap configurations

### 8. Find on Screen

Ctrl+F text search within the current screen buffer, with highlighting. Simple but very useful on dense 80-column displays.

**Key features:**
- Search bar overlay (non-intrusive)
- Highlight all matches on screen
- Navigate between matches with Enter/Shift+Enter
- Case-sensitive and regex options
- Persist last search term

### 9. Font Scaling / Zoom

Ctrl+/- zoom in/out for accessibility, auto-fitting to window size.

**Key features:**
- Keyboard shortcuts: Ctrl+Plus, Ctrl+Minus, Ctrl+0 (reset)
- Mouse wheel zoom with Ctrl held
- Auto-fit mode: scale font to fill window
- Minimum/maximum zoom bounds
- Per-session zoom level persistence

---

## Tier 3 — Advanced / Differentiation

### 10. SSH Tunneling

Built-in SSH tunnel configuration so users don't need external tools for secure access to systems without native TLS.

**Key features:**
- SSH tunnel setup integrated into connection dialog
- Support for password and key-based authentication
- Local port forwarding configuration
- Jump host / bastion host support
- Tunnel status indicator in status bar

### 11. Additional Code Pages

Broader international support with additional EBCDIC code pages.

**Target code pages:**
- CP870 — Eastern Europe (Latin-2)
- CP420 — Arabic
- CP424 — Hebrew
- CP838 — Thai
- CP930 / CP939 — Japanese (DBCS)
- CP933 — Korean (DBCS)
- CP935 / CP937 — Simplified / Traditional Chinese (DBCS)

### 12. Session Logging & Audit Trail

Log all screen transitions and keystrokes to a file for compliance/auditing. Timestamp each interaction.

**Key features:**
- Per-session log files with timestamps
- Configurable verbosity (screens only, screens + keystrokes, full protocol)
- Log rotation and retention policies
- Export to common formats (plain text, HTML with colors)
- Tamper-evident logging option (checksums)

### 13. Multi-Host Connection Profiles with Folders

Organize saved sessions into groups/folders for users managing many AS/400 systems.

**Key features:**
- Tree-view session organizer in connect dialog
- Drag-and-drop folder organization
- Color-coded or icon-tagged groups (production, development, test)
- Quick-connect favorites bar
- Session search/filter

### 14. Clipboard Integration Enhancements

Advanced clipboard operations for working with terminal data.

**Key features:**
- Paste with EBCDIC-aware conversion
- Rectangular block selection (column mode)
- "Paste as keystrokes" for field-aware input (respects field boundaries)
- Copy with formatting (HTML/RTF with colors)
- Clipboard history ring

### 15. Status Bar Indicators

Show contextual information extracted from screen content heuristics.

**Key features:**
- Current AS/400 system name detection
- Active job name / program name display
- Message waiting indicator
- Keyboard lock state with reason
- Connection latency / round-trip time display

---

## Tier 4 — Polish (Phase 7 Aligned)

### 16. Platform Installers

- `.deb` and `.rpm` packages for Linux distributions
- macOS `.dmg` with drag-to-Applications installer
- Windows `.msi` installer with code signing
- Flatpak / Snap for Linux universal packaging
- Automated build pipeline for all platforms

### 17. Auto-Update Mechanism

- Check for new versions on startup (configurable)
- Notify user of available updates
- One-click update or link to download page
- Changelog display for new versions
- Update channel selection (stable, beta)

### 18. Accessibility (a11y)

- Screen reader support (Qt Accessibility API)
- High-DPI scaling and display density awareness
- Keyboard-only navigation through all UI elements
- Configurable color contrast ratios
- Large cursor and pointer options

---

## Recommended Starting Point

**Macro Recording (#1)** and **Hotspots (#4)** offer the best return on investment:

- Both are self-contained and don't require protocol changes
- Both dramatically improve daily usability for AS/400 operators
- Macros address the #1 pain point: repetitive navigation
- Hotspots modernize the interaction model without breaking the terminal paradigm
- Both can be implemented incrementally alongside existing features
