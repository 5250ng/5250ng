# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Phase 5 UX and Sessions: Complete user interface and session management
  - SessionConfig class for storing connection and display settings
  - SessionManager for persistent session profile management (save/load/delete)
  - ConnectDialog UI for connection configuration with session management
  - Logger system with file and console output, log levels (Debug/Info/Warning/Error)
  - Main window integration with connect dialog, status bar, and menu bar
  - Session persistence using JSON files in application data directory
  - Unit tests for session configuration and serialization
- Phase 4 Input Layer: Complete keyboard input handling
  - KeyboardEncoder class for converting Qt key events to TN5250 protocol format
  - PF-key mapping system (F1-F24) with proper AID codes
  - Special key handling (Enter, Tab, Backspace, Arrow keys, etc.)
  - InputHandler class for field navigation and editing
  - Field navigation (next/previous field, field start/end)
  - Cursor movement and character insertion/deletion
  - Protected field validation
  - Integration with TN5250Widget for keyboard event capture
  - Unit tests for keyboard encoder and input handler
- Phase 3 Display Engine: Complete rendering system
  - ScreenBuffer class for display data management with cursor and field support
  - TN5250Widget custom QWidget with QPainter-based rendering
  - EBCDIC to UTF-8 conversion utilities (Code Page 037)
  - Attribute handling (color, reverse video, blink, underline)
  - Cursor rendering with blinking support
  - Field management (protected/unprotected fields)
  - Screen scrolling (up/down)
  - Support for 27×132 extended mode
  - Unit tests for screen buffer, EBCDIC conversion, and attribute rendering
- Phase 2 Transport Layer: Complete TN5250 client implementation
  - TN5250Client class with Qt6 socket support
  - Telnet option negotiation (IAC, DO, DONT, WILL, WONT)
  - TN5250 handshake protocol (RFC 1205 based)
  - TLS/SSL support (optional, requires Qt6 SSL)
  - Protocol parser for TN5250 data streams
  - Unit tests for protocol parser (state transitions, command parsing)
  - Integration tests for socket connection and handshake
- Phase 1 Foundation: Repository structure with layered architecture (Core/Transport/Display/UI/AI)
- CMake build system with Qt6 integration
- CI pipeline configuration for Linux, macOS, and Windows
- Basic project structure and directory organization
- .gitignore for build artifacts and IDE files

### Changed
- Reorganized source files into modular layer structure
- Enhanced CMakeLists.txt with proper configuration and build options
- Added test framework with Qt6 Test

## [0.5.0] - Phase 5 UX and Sessions

Complete user interface with connect dialog, session management, and logging system.

## [0.4.0] - Phase 4 Input Layer

Complete input layer with keyboard encoding, PF-key mapping, field navigation, and input handling.

## [0.3.0] - Phase 3 Display Engine

Complete display engine with screen buffer, custom rendering widget, EBCDIC conversion, and full attribute support.

## [0.2.0] - Phase 2 Transport Layer

Complete transport layer implementation with socket client, telnet negotiation, TN5250 handshake, and protocol parsing.

## [0.1.0] - Phase 1 Foundation

Initial project setup and foundation work.

