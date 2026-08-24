cmake_minimum_required(VERSION 3.16)
project(5250ng VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Build options
option(BUILD_TESTS "Build unit and integration tests" ON)

# Qt6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

find_package(OpenSSL REQUIRED)

# TLS support gate. QSslSocket ships inside Qt6::Network (there is no
# separate Qt6::Ssl module), but some Qt builds are compiled without SSL.
# Probe that the header is present and usable; the TLS client code paths
# (#ifdef HAVE_QT6_SSL) are compiled in only when this succeeds. Whether an
# SSL backend is actually loadable is still checked at runtime via
# QSslSocket::supportsSsl().
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES Qt6::Network)
check_cxx_source_compiles("
#include <QSslSocket>
int main() { return QSslSocket::supportsSsl() ? 0 : 1; }
" HAVE_QT6_SSL)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT HAVE_QT6_SSL)
    message(WARNING "Qt6 Network was built without SSL support - TLS connections will be disabled")
endif()

# Generate version header from project version
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

# Protocol library (standalone, Qt-free)
set(TN5250_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(src/network/tn5250)

# 5250script interpreter library (submodule)
set(BUILD_5250SCRIPT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/5250script)

# Source files organized by layer
set(CORE_SOURCES
    src/core/ebcdic.cpp
    src/core/ebcdic.h
    src/core/codepage.cpp
    src/core/codepage.h
    src/core/aid_key_map.h
    src/core/keyboard_encoder.cpp
    src/core/keyboard_encoder.h
    src/core/keyboard_mapping.cpp
    src/core/keyboard_mapping.h
    src/core/hotspot_detector.cpp
    src/core/hotspot_detector.h
    src/core/screen_history.cpp
    src/core/screen_history.h
    src/core/macro_recorder.cpp
    src/core/macro_recorder.h
    src/core/macro_config.cpp
    src/core/macro_config.h
    src/core/session_logger.cpp
    src/core/session_logger.h
    src/core/match_replace_engine.cpp
    src/core/match_replace_engine.h
    src/core/macro_to_script.cpp
    src/core/macro_to_script.h
    src/core/command_runner.cpp
    src/core/command_runner.h
    src/core/pcap_replay.cpp
    src/core/pcap_replay.h
)

set(SESSION_SOURCES
    src/session/config.cpp
    src/session/config.h
    src/session/manager.cpp
    src/session/manager.h
    src/session/worker.cpp
    src/session/worker.h
)

set(LOGGER_SOURCES
    src/logger/logger.cpp
    src/logger/logger.h
)

# TN5250 Qt sources: old client files (still use Qt types) + new decoder adapter
# The old telnet, devices, ibmrseed files are kept because client_*.cpp use
# the Qt-typed versions. The protocol library provides the standalone equivalents.
set(TELNET_SOURCES
    src/network/tn5250_qt/telnet/commands.cpp
    src/network/tn5250_qt/telnet/commands.h
    src/network/tn5250_qt/telnet/options.cpp
    src/network/tn5250_qt/telnet/options.h
    src/network/tn5250_qt/telnet/client.cpp
    src/network/tn5250_qt/telnet/client.h
    src/network/tn5250_qt/devices/devices.cpp
    src/network/tn5250_qt/devices/devices.h
)

set(TN5250_ADAPTER_SOURCES
    src/network/tn5250_qt/client/client.h
    src/network/tn5250_qt/client/client_connection.cpp
    src/network/tn5250_qt/client/client_io.cpp
    src/network/tn5250_qt/client/client_telnet.cpp
    src/network/tn5250_qt/client/client_handshake.cpp
    src/network/tn5250_qt/client/client_tls.cpp
    src/network/tn5250_qt/client/ibmrseed.cpp
    src/network/tn5250_qt/client/ibmrseed.h
    src/network/tn5250_qt/client/decoder_adapter.h
    src/network/tn5250_qt/client/decoder_adapter.cpp
)

set(HOSTSERVER_SOURCES
    src/network/hostserver/host_constants.h
    src/network/hostserver/host_data_stream.cpp
    src/network/hostserver/host_data_stream.h
    src/network/hostserver/service_mapper.cpp
    src/network/hostserver/service_mapper.h
    src/network/hostserver/password_encrypt.cpp
    src/network/hostserver/password_encrypt.h
    src/network/hostserver/signon_client.cpp
    src/network/hostserver/signon_client.h
)

set(AGENT_SOURCES
    src/agent/provider.h
    src/agent/provider.cpp
    src/agent/config.h
    src/agent/config.cpp
    src/agent/tool_call.h
    src/agent/tool_definitions.h
    src/agent/tool_definitions.cpp
    # MCP server
    src/mcp/McpServer.h
    src/mcp/McpServer.cpp
    src/mcp/McpSessionRegistry.h
    src/mcp/McpSessionRegistry.cpp
    src/mcp/McpToolHandler.h
    src/mcp/McpToolHandler.cpp
    src/mcp/McpHttpParser.h
    src/mcp/McpHttpParser.cpp
    src/agent/agent_script_runner.h
    src/agent/agent_script_runner.cpp
    src/agent/script_generator.h
    src/agent/script_generator.cpp
    src/agent/prompts/prompts.qrc
    src/agent/auth/auth_method.h
    src/agent/auth/api_key_auth.h
    src/agent/auth/api_key_auth.cpp
    src/agent/auth/oauth_config.h
    src/agent/auth/oauth_auth.h
    src/agent/auth/oauth_auth.cpp
    src/agent/auth/oauth_flow.h
    src/agent/auth/oauth_flow.cpp
    src/agent/auth/token_storage.h
    src/agent/auth/token_storage.cpp
    src/agent/providers/openai_provider.h
    src/agent/providers/openai_provider.cpp
    src/agent/markdown_converter.h
    src/agent/markdown_converter.cpp
    src/agent/providers/anthropic_provider.h
    src/agent/providers/anthropic_provider.cpp
)

set(UTILS_SOURCES
    src/utils/endianness/big_endian.cpp
    src/utils/endianness/big_endian.h
    src/utils/endianness/little_endian.cpp
    src/utils/endianness/little_endian.h
    src/utils/binary/binary.cpp
    src/utils/binary/binary.h
    src/utils/hex/hex.cpp
    src/utils/hex/hex.h
)

set(DISPLAY_SOURCES
    src/ui/rendering/gddm_5292_decoder.cpp
    src/ui/rendering/gddm_5292_decoder.h
    src/ui/widgets/Q5250ScreenWidget/screen_buffer.cpp
    src/ui/widgets/Q5250ScreenWidget/screen_buffer.h
    src/ui/widgets/Q5250ScreenWidget/screen_buffer_adapter.cpp
    src/ui/widgets/Q5250ScreenWidget/screen_buffer_adapter.h
    src/ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.cpp
    src/ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h
    src/ui/widgets/Q5250ScreenWidget/Q5250Cursor.cpp
    src/ui/widgets/Q5250ScreenWidget/Q5250Cursor.h
    src/ui/widgets/Q5250ScreenWidget/Q5250HRule.cpp
    src/ui/widgets/Q5250ScreenWidget/Q5250HRule.h
    src/ui/widgets/Q5250ScreenWidget/Q5250TerminalView.cpp
    src/ui/widgets/Q5250ScreenWidget/Q5250TerminalView.h
    src/ui/widgets/Q5250ScreenWidget/events.cpp
    src/ui/widgets/Q5250ScreenWidget/actions.cpp
    src/ui/widgets/Q5250ScreenWidget/size.cpp
    src/ui/widgets/Q5250ScreenWidget/cursor.cpp
    src/ui/widgets/Q5250ScreenWidget/utils.cpp
    src/ui/widgets/Q5250ScreenWidget/selection.cpp
)

set(UI_SOURCES
    # main window
    src/ui/main_window.cpp
    src/ui/main_window.h
    # rendering (extracted from main_window)
    src/ui/rendering/tn5250_stream_renderer.cpp
    src/ui/rendering/tn5250_stream_renderer.h
    src/ui/rendering/tn5250_command_handler.cpp
    src/ui/rendering/tn5250_command_handler.h
    src/ui/rendering/display_attributes.h
    # devices
    src/ui/device/IBM-3179-2.cpp
    src/ui/device/IBM-3179-2.h
    # widgets
    src/ui/widgets/QBackgroundImageWidget/QBackgroundImageWidget.cpp
    src/ui/widgets/QBackgroundImageWidget/QBackgroundImageWidget.h
    src/ui/widgets/QAgentPanelWidget/QAgentPanelWidget.cpp
    src/ui/widgets/QAgentPanelWidget/QAgentPanelWidget.h
    src/ui/widgets/QCRTOverlayWidget/QCRTOverlayWidget.cpp
    src/ui/widgets/QCRTOverlayWidget/QCRTOverlayWidget.h
    src/ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.cpp
    src/ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.h
    src/ui/widgets/QVirtualKeyboardWidget/QVirtualKeyboardWidget.cpp
    src/ui/widgets/QVirtualKeyboardWidget/QVirtualKeyboardWidget.h
    src/ui/widgets/Frameless/BaseFramelessWindow.cpp
    src/ui/widgets/Frameless/BaseFramelessWindow.h
    src/ui/widgets/Frameless/BaseFramelessDialog.cpp
    src/ui/widgets/Frameless/BaseFramelessDialog.h
    src/ui/widgets/Frameless/TitleBar.cpp
    src/ui/widgets/Frameless/TitleBar.h
    src/ui/widgets/Frameless/StyledMessageBox.cpp
    src/ui/widgets/Frameless/StyledMessageBox.h
    src/ui/widgets/Frameless/StyledInputDialog.cpp
    src/ui/widgets/Frameless/StyledInputDialog.h
    src/ui/widgets/Frameless/StyledColorDialog.cpp
    src/ui/widgets/Frameless/StyledColorDialog.h
    src/ui/widgets/Frameless/StyledFileDialog.cpp
    src/ui/widgets/Frameless/StyledFileDialog.h
    # dialogs
    src/ui/dialogs/log_viewer.cpp
    src/ui/dialogs/log_viewer.h
    src/ui/dialogs/mcp_log_viewer.h
    src/ui/dialogs/mcp_log_viewer.cpp
    src/ui/dialogs/connect_dialog.cpp
    src/ui/dialogs/connect_dialog.h
    src/ui/dialogs/settings_dialog.cpp
    src/ui/dialogs/settings_dialog.h
    src/ui/dialogs/session_settings_dialog.cpp
    src/ui/dialogs/session_settings_dialog.h
    src/ui/dialogs/theme_preview_widget.cpp
    src/ui/dialogs/theme_preview_widget.h
    src/ui/dialogs/match_replace_dialog.cpp
    src/ui/dialogs/match_replace_dialog.h
    src/ui/dialogs/keyboard_remap_dialog.cpp
    src/ui/dialogs/keyboard_remap_dialog.h
    src/ui/dialogs/pc_command_confirm_dialog.cpp
    src/ui/dialogs/pc_command_confirm_dialog.h
    # themes
    src/ui/themes/manager.cpp
    src/ui/themes/manager.h
    src/ui/themes/font_loader.cpp
    src/ui/themes/font_loader.h
    src/ui/themes/terminal_theme.cpp
    src/ui/themes/terminal_theme.h
    src/ui/themes/terminal_theme_manager.cpp
    src/ui/themes/terminal_theme_manager.h
    src/ui/themes/themes.qrc
    # actions
    src/ui/actions/onAbout.cpp
    src/ui/actions/onCloseTabRequested.cpp
    src/ui/actions/onConnect.cpp
    src/ui/actions/onConnectRequested.cpp
    src/ui/actions/onCurrentTabChanged.cpp
    src/ui/actions/onDisconnect.cpp
    src/ui/actions/onLogMessage.cpp
    src/ui/actions/onNewSession.cpp
    src/ui/actions/onOpenSettings.cpp
    src/ui/actions/onQuickOpenChanged.cpp
    src/ui/actions/onQuickTheme.cpp
    src/ui/actions/onRenameTabRequested.cpp
    src/ui/actions/onSavedSessionChosen.cpp
    src/ui/actions/onTabMoved.cpp
    src/ui/actions/onTakeScreenshot.cpp
    src/ui/actions/onReconnect.cpp
    src/ui/actions/onDuplicateSession.cpp
    src/ui/actions/onCopySessionGuid.cpp
    src/ui/actions/onToggleFullscreen.cpp
    src/ui/actions/onEditActions.cpp
    src/ui/actions/onHotspots.cpp
    src/ui/actions/onScripts.cpp
    src/ui/actions/onSessionLogging.cpp
    src/ui/actions/onScreenHistory.cpp
    src/ui/actions/onToggleAgentPanel.cpp
    src/ui/actions/onMatchReplace.cpp
    src/ui/actions/onVirtualKeyboard.cpp
    src/ui/actions/onReplayPcap.cpp
    # setup
    src/ui/setup/setupUI.cpp
    src/ui/setup/setupMenuBar.cpp
    src/ui/setup/setupStatusBar.cpp
)

# Windows resource file (icon, version info, manifest)
if(WIN32)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/5250ng.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/5250ng.rc
        @ONLY
    )
    # Copy icon and manifest next to the generated .rc so the resource compiler finds them
    file(COPY
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/5250ng.ico
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/5250ng.manifest
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    )
    set(WIN32_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/5250ng.rc)
endif()

# Main executable
add_executable(5250ng
    src/main.cpp
    resources/resources.qrc
    ${UI_SOURCES}
    ${TELNET_SOURCES}
    ${TN5250_ADAPTER_SOURCES}
    ${HOSTSERVER_SOURCES}
    ${SESSION_SOURCES}
    ${DISPLAY_SOURCES}
    ${CORE_SOURCES}
    ${LOGGER_SOURCES}
    ${UTILS_SOURCES}
    ${AGENT_SOURCES}
    ${WIN32_RESOURCES}
)

target_link_libraries(5250ng
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        OpenSSL::Crypto
        tn5250::tn5250
        5250script
)

# QSslSocket lives in Qt6::Network (already linked); only the define is needed
if(HAVE_QT6_SSL)
    target_compile_definitions(5250ng PRIVATE HAVE_QT6_SSL)
endif()

# Include directories
target_include_directories(5250ng PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Set output directory
set_target_properties(5250ng PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Testing (when enabled)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # Shared static library — compiled once, linked by all test targets
    # Theme sources needed by Q5250ScreenWidget (applyTerminalTheme)
    set(THEME_SOURCES
        src/ui/themes/terminal_theme.cpp
        src/ui/themes/terminal_theme.h
    )

    add_library(tn5250_core STATIC
        ${CORE_SOURCES}
        ${SESSION_SOURCES}
        ${TELNET_SOURCES}
        ${TN5250_ADAPTER_SOURCES}
        ${HOSTSERVER_SOURCES}
        ${LOGGER_SOURCES}
        ${UTILS_SOURCES}
        ${DISPLAY_SOURCES}
        ${THEME_SOURCES}
    )
    target_link_libraries(tn5250_core
        PUBLIC
            Qt6::Core
            Qt6::Network
            Qt6::Widgets
            OpenSSL::Crypto
            tn5250::tn5250
            5250script
    )
    if(HAVE_QT6_SSL)
        target_compile_definitions(tn5250_core PUBLIC HAVE_QT6_SSL)
    endif()
    target_include_directories(tn5250_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    # Helper macro to define a test target
    macro(add_tn5250_test TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE tn5250_core Qt6::Test)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endmacro()

    # Unit tests
    add_tn5250_test(test_decoder tests/unit/test_decoder.cpp)
    add_tn5250_test(test_ebcdic tests/unit/test_ebcdic.cpp)
    add_tn5250_test(test_screen_buffer tests/unit/test_screen_buffer.cpp)
    add_tn5250_test(test_keyboard_encoder tests/unit/test_keyboard_encoder.cpp)
    add_tn5250_test(test_keyboard_mapping tests/unit/test_keyboard_mapping.cpp)
    add_tn5250_test(test_aid_key_map tests/unit/test_aid_key_map.cpp)
    add_tn5250_test(test_session_config tests/unit/test_session_config.cpp)
    add_tn5250_test(test_command_runner tests/unit/test_command_runner.cpp)

    add_tn5250_test(test_ibmrseed tests/unit/test_ibmrseed.cpp)
    add_tn5250_test(test_telnet_subnegotiation tests/unit/test_telnet_subnegotiation.cpp)
    add_tn5250_test(test_logger tests/unit/test_logger.cpp)
    add_tn5250_test(test_codepage tests/unit/test_codepage.cpp)

    add_tn5250_test(test_terminal_theme tests/unit/test_terminal_theme.cpp)
    add_tn5250_test(test_pcap_replay tests/unit/test_pcap_replay.cpp)
    add_tn5250_test(test_match_replace tests/unit/test_match_replace.cpp)
    add_tn5250_test(test_gddm_5292 tests/unit/test_gddm_5292.cpp)

    # Host server tests
    add_tn5250_test(test_host_data_stream tests/unit/test_host_data_stream.cpp)
    add_tn5250_test(test_password_encrypt tests/unit/test_password_encrypt.cpp)

    # Command handler tests
    # TN5250CommandHandler / TN5250StreamRenderer live in the main
    # application target (not tn5250_core), so compile them into the test
    # binary directly.
    add_executable(test_command_handler_soh
        tests/unit/test_command_handler_soh.cpp
        src/ui/rendering/tn5250_command_handler.cpp
        src/ui/rendering/tn5250_stream_renderer.cpp
        src/ui/dialogs/pc_command_confirm_dialog.cpp
        src/ui/widgets/Frameless/BaseFramelessDialog.cpp
        src/ui/widgets/Frameless/TitleBar.cpp
        src/ui/themes/manager.cpp)
    target_link_libraries(test_command_handler_soh PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_command_handler_soh COMMAND test_command_handler_soh)

    # MCP transport tests
    # McpHttpParser lives in the main application target (not tn5250_core),
    # so compile it into the test binary directly.
    add_executable(test_mcp_http_parser
        tests/unit/test_mcp_http_parser.cpp
        src/mcp/McpHttpParser.cpp)
    target_link_libraries(test_mcp_http_parser PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_mcp_http_parser COMMAND test_mcp_http_parser)
    add_tn5250_test(test_mcp_validation tests/unit/test_mcp_validation.cpp)

    # Scripting tests
    add_tn5250_test(test_script_lexer tests/unit/test_script_lexer.cpp)
    add_tn5250_test(test_script_escape tests/unit/test_script_escape.cpp)
    add_tn5250_test(test_script_parser tests/unit/test_script_parser.cpp)

    # Integration tests
    add_tn5250_test(test_tn5250_connection tests/integration/test_tn5250_connection.cpp)
    add_tn5250_test(test_tn5250_tls_connection tests/integration/test_tn5250_tls_connection.cpp)

    message(STATUS "Testing enabled - unit and integration tests configured")
endif()

# Installation (for future phases)
install(TARGETS 5250ng
    RUNTIME DESTINATION bin
)

# Platform-specific settings
if(WIN32)
    set_target_properties(5250ng PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    # The .rc file embeds our custom manifest (DPI awareness, Win10/11 compat,
    # UAC asInvoker, common-controls v6). MSVC's linker also auto-generates a
    # manifest at id 1, language 0x0409 — the same slot — which causes
    # CVT1100 "duplicate resource" at link time. Suppress the linker's
    # manifest so only the .rc-embedded one ships.
    if(MSVC)
        target_link_options(5250ng PRIVATE "/MANIFEST:NO")
    endif()
endif()

if(APPLE)
    set_target_properties(5250ng PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "5250ng"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.p0dalirius.5250ng"
    )
endif()
