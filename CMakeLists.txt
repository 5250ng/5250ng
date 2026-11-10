cmake_minimum_required(VERSION 3.16)
# Version resolution.  The version is derived from the newest reachable git
# tag (vX.Y.Z convention) via `git describe`, so a tagged checkout reports its
# real version (Pomoc -> About, --version, Windows resources, macOS bundle)
# without editing this file.  Falls back to 0.5.0 when git or tags are
# unavailable (tarballs, forks).
set(PROJECT_GIT_FALLBACK_VERSION "0.5.0")
find_package(Git QUIET)
set(APP_DISPLAY_VERSION "${PROJECT_GIT_FALLBACK_VERSION}")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_DESCRIBE_RESULT
    )
    if(GIT_DESCRIBE_RESULT EQUAL 0)
        # Strip the leading "v" and any commit/dirty suffix so that
        # project(VERSION ...) receives a plain X.Y.Z.
        string(REGEX REPLACE "^v" "" APP_DISPLAY_VERSION "${GIT_DESCRIBE_VERSION}")
        string(REGEX REPLACE "[-+].*$" "" APP_DISPLAY_VERSION "${APP_DISPLAY_VERSION}")
    else()
        message(STATUS "Version: no git tags found, falling back to ${APP_DISPLAY_VERSION}")
    endif()
endif()
message(STATUS "Version: ${APP_DISPLAY_VERSION}")

project(5250ng VERSION ${APP_DISPLAY_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Build options
option(BUILD_TESTS "Build unit and integration tests" ON)

# Keep release bundles runnable on currently supported Intel and Apple Silicon
# Macs.  Callers can override this when targeting a different deployment
# baseline.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
        "Minimum macOS version supported by the application")
endif()

# Qt6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

find_package(QtUiStyle 0.1 CONFIG QUIET)
if(NOT TARGET QtUiStyle::Dialogs)
    include(FetchContent)
    set(QT_UI_STYLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(QT_UI_STYLE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(QtUiStyle
        GIT_REPOSITORY https://github.com/5250ng/qt-ui-style.git
        GIT_TAG v0.1.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(QtUiStyle)
endif()

# Avoid a Homebrew-only libcrypto path in distributable macOS bundles.  The
# application uses OpenSSL::Crypto directly; linking that dependency statically
# leaves Qt's own TLS backend to use the platform/backend libraries it deploys.
if(APPLE)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
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
    src/core/script_config.cpp
    src/core/script_config.h
    src/core/session_logger.cpp
    src/core/session_logger.h
    src/core/match_replace_engine.cpp
    src/core/match_replace_engine.h
    src/core/macro_to_script.cpp
    src/core/macro_to_script.h
    src/core/script_library.cpp
    src/core/script_library.h
    src/core/settings_permissions.h
    src/core/system_name.h
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
    src/mcp/mcp_request_guard.h
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
    # script editor
    src/ui/widgets/QScriptEditorWidget/QScriptEditorWidget.cpp
    src/ui/widgets/QScriptEditorWidget/QScriptEditorWidget.h
    src/ui/widgets/QScriptEditorWidget/ScriptCodeEditor.cpp
    src/ui/widgets/QScriptEditorWidget/ScriptCodeEditor.h
    src/ui/widgets/QScriptEditorWidget/ScriptSyntaxHighlighter.cpp
    src/ui/widgets/QScriptEditorWidget/ScriptSyntaxHighlighter.h
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
    # dialogs
    src/ui/dialogs/log_viewer.cpp
    src/ui/dialogs/log_viewer.h
    src/ui/dialogs/script_editor_window.cpp
    src/ui/dialogs/script_editor_window.h
    src/ui/dialogs/script_editor_dialog.cpp
    src/ui/dialogs/script_editor_dialog.h
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
    src/ui/dialogs/script_input_dialog.cpp
    src/ui/dialogs/script_input_dialog.h
    src/ui/dialogs/mcp_password_prompt_dialog.cpp
    src/ui/dialogs/mcp_password_prompt_dialog.h
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
    src/ui/actions/onSaveSession.cpp
    src/ui/actions/onScriptEditor.cpp
    src/ui/actions/onCopySessionGuid.cpp
    src/ui/actions/onToggleFullscreen.cpp
    src/ui/actions/onEditActions.cpp
    src/ui/actions/onHotspots.cpp
    src/ui/actions/onScripts.cpp
    src/ui/actions/onSessionLogging.cpp
    src/ui/actions/onDiagnostics.cpp
    src/ui/actions/onScreenHistory.cpp
    src/ui/actions/onToggleAgentPanel.cpp
    src/ui/actions/onMatchReplace.cpp
    src/ui/actions/onVirtualKeyboard.cpp
    src/ui/actions/onReplayPcap.cpp
    # setup
    src/ui/setup/setupUI.cpp
    src/ui/widgets/popup_choice_filter.h
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

# macOS application icon.  The checked-in PNG is also used by Qt at runtime;
# generate Apple's multi-resolution .icns container as part of a macOS build.
if(APPLE)
    set(MACOS_ICON ${CMAKE_CURRENT_BINARY_DIR}/5250ng.icns)
    add_custom_command(
        OUTPUT ${MACOS_ICON}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_PNG=${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/5250ng.png
            -DOUTPUT_ICNS=${MACOS_ICON}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateMacIcon.cmake
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/5250ng.png
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateMacIcon.cmake
        COMMENT "Generating macOS application icon"
        VERBATIM
    )
    set_source_files_properties(${MACOS_ICON} PROPERTIES
        GENERATED TRUE
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/LICENSE PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set(MACOS_RESOURCES
        ${MACOS_ICON}
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    )
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
    ${MACOS_RESOURCES}
)

target_link_libraries(5250ng
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        OpenSSL::Crypto
        tn5250::tn5250
        5250script
        QtUiStyle::Dialogs
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
    add_tn5250_test(test_agent_collapsible_preview tests/unit/test_agent_collapsible_preview.cpp)
    add_tn5250_test(test_screen_buffer tests/unit/test_screen_buffer.cpp)
    add_tn5250_test(test_screen_history_snapshots tests/unit/test_screen_history_snapshots.cpp)
    add_tn5250_test(test_history_view tests/unit/test_history_view.cpp)

    add_tn5250_test(test_system_name tests/unit/test_system_name.cpp)
    add_tn5250_test(test_keyboard_encoder tests/unit/test_keyboard_encoder.cpp)
    add_tn5250_test(test_input_gate tests/unit/test_input_gate.cpp)
    add_tn5250_test(test_menu_hotspots tests/unit/test_menu_hotspots.cpp)
    add_tn5250_test(test_history_nondisplay tests/unit/test_history_nondisplay.cpp)
    add_tn5250_test(test_backtab_shift tests/unit/test_backtab_shift.cpp)
    add_tn5250_test(test_copy_nondisplay tests/unit/test_copy_nondisplay.cpp)
    add_tn5250_test(test_typing_outside_fields tests/unit/test_typing_outside_fields.cpp)

    add_tn5250_test(test_paste_input_rules tests/unit/test_paste_input_rules.cpp)

    add_tn5250_test(test_shortcut_override tests/unit/test_shortcut_override.cpp)
    add_tn5250_test(test_auto_enter_aid tests/unit/test_auto_enter_aid.cpp)
    add_tn5250_test(test_keyboard_mapping tests/unit/test_keyboard_mapping.cpp)
    add_tn5250_test(test_macro_chords tests/unit/test_macro_chords.cpp)
    add_tn5250_test(test_popup_choice_filter tests/unit/test_popup_choice_filter.cpp)
    add_tn5250_test(test_aid_key_map tests/unit/test_aid_key_map.cpp)
    add_tn5250_test(test_unbound_key tests/unit/test_unbound_key.cpp)
    add_tn5250_test(test_wheel_roll tests/unit/test_wheel_roll.cpp)
    add_tn5250_test(test_script_local_keys tests/unit/test_script_local_keys.cpp)
    add_tn5250_test(test_session_config tests/unit/test_session_config.cpp)
    add_tn5250_test(test_session_save_atomic tests/unit/test_session_save_atomic.cpp)
    add_tn5250_test(test_script_editor tests/unit/test_script_editor.cpp)
    add_tn5250_test(test_script_config tests/unit/test_script_config.cpp)
    add_tn5250_test(test_command_runner tests/unit/test_command_runner.cpp)
    add_tn5250_test(test_script_metadata_quotes tests/unit/test_script_metadata_quotes.cpp)

    add_tn5250_test(test_ibmrseed tests/unit/test_ibmrseed.cpp)
    add_tn5250_test(test_telnet_subnegotiation tests/unit/test_telnet_subnegotiation.cpp)
    add_tn5250_test(test_telnet_option_state tests/unit/test_telnet_option_state.cpp)
    add_tn5250_test(test_logger tests/unit/test_logger.cpp)
    add_tn5250_test(test_session_log_file_names tests/unit/test_session_log_file_names.cpp)
    add_tn5250_test(test_codepage tests/unit/test_codepage.cpp)
    add_tn5250_test(test_codepage_new_environ tests/unit/test_codepage_new_environ.cpp)

    add_tn5250_test(test_terminal_theme tests/unit/test_terminal_theme.cpp)
    add_tn5250_test(test_pcap_replay tests/unit/test_pcap_replay.cpp)
    add_tn5250_test(test_match_replace tests/unit/test_match_replace.cpp)
    add_tn5250_test(test_match_replace_hidden_cells tests/unit/test_match_replace_hidden_cells.cpp)
    add_tn5250_test(test_match_replace_rebuild tests/unit/test_match_replace_rebuild.cpp)
    add_tn5250_test(test_gddm_5292 tests/unit/test_gddm_5292.cpp)
    add_tn5250_test(test_gddm_5292_fuzz tests/unit/test_gddm_5292_fuzz.cpp)

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
    )
    target_link_libraries(test_command_handler_soh PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_command_handler_soh COMMAND test_command_handler_soh)

    # Write Error Code after a WTD unlock stays error-locked (issue #238)
    add_executable(test_write_error_code_lock
        tests/unit/test_write_error_code_lock.cpp
        src/ui/rendering/tn5250_command_handler.cpp
        src/ui/rendering/tn5250_stream_renderer.cpp
        src/ui/dialogs/pc_command_confirm_dialog.cpp
    )
    target_link_libraries(test_write_error_code_lock PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_write_error_code_lock COMMAND test_write_error_code_lock)

    # 5250 Query reply layout (issue #247)
    add_executable(test_query_reply
        tests/unit/test_query_reply.cpp
        src/ui/rendering/tn5250_command_handler.cpp
        src/ui/rendering/tn5250_stream_renderer.cpp
        src/ui/dialogs/pc_command_confirm_dialog.cpp
    )
    target_link_libraries(test_query_reply PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_query_reply COMMAND test_query_reply)

    # Stream renderer: non-display input fields (issue #360)
    add_executable(test_stream_renderer_nondisplay
        tests/unit/test_stream_renderer_nondisplay.cpp
        src/ui/rendering/tn5250_stream_renderer.cpp
    )
    target_link_libraries(test_stream_renderer_nondisplay PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_stream_renderer_nondisplay COMMAND test_stream_renderer_nondisplay)

    # STRPCCMD on unattended (read-only) sessions (issue #251)
    add_executable(test_strpccmd_unattended
        tests/unit/test_strpccmd_unattended.cpp
        src/ui/rendering/tn5250_command_handler.cpp
        src/ui/rendering/tn5250_stream_renderer.cpp
        src/ui/dialogs/pc_command_confirm_dialog.cpp
    )
    target_link_libraries(test_strpccmd_unattended PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_strpccmd_unattended COMMAND test_strpccmd_unattended)

    # Theme import/export file paths (TerminalThemeManager lives in the
    # application target)
    add_executable(test_theme_import_paths
        tests/unit/test_theme_import_paths.cpp
        src/ui/themes/terminal_theme_manager.cpp
    )
    target_link_libraries(test_theme_import_paths PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_theme_import_paths COMMAND test_theme_import_paths)

    # MCP login tool (McpToolHandler and the script runner live in the
    # application target, so compile them into the test binary directly).
    add_executable(test_mcp_login
        tests/unit/test_mcp_login.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_login PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_login COMMAND test_mcp_login)

    # MCP reads use the session's own code page (issue #481)
    add_executable(test_mcp_session_code_page
        tests/unit/test_mcp_session_code_page.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_session_code_page PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_session_code_page COMMAND test_mcp_session_code_page)

    # read_region bounds without overflow (issue #406)
    add_executable(test_read_region_bounds
        tests/unit/test_read_region_bounds.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_read_region_bounds PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_read_region_bounds COMMAND test_read_region_bounds)

    # Non-display cells in MCP and script screen reads (issue #381)
    add_executable(test_nondisplay_exports
        tests/unit/test_nondisplay_exports.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_nondisplay_exports PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_nondisplay_exports COMMAND test_nondisplay_exports)

    # Per-session menu items follow the active tab (real MainWindow)
    add_executable(test_menu_check_state
        tests/integration/test_menu_check_state.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_menu_check_state PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_menu_check_state PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_menu_check_state PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_menu_check_state COMMAND test_menu_check_state)

    # Settings dialog Save (application sources)
    add_executable(test_settings_save_all_pages
        tests/unit/test_settings_save_all_pages.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_settings_save_all_pages PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_settings_save_all_pages PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_settings_save_all_pages PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_settings_save_all_pages COMMAND test_settings_save_all_pages)

    # MCP server stop must drop connected clients
    add_executable(test_mcp_server_stop
        tests/unit/test_mcp_server_stop.cpp
        src/mcp/McpServer.cpp
        src/mcp/McpHttpParser.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_server_stop PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_server_stop COMMAND test_mcp_server_stop)

    # MCP request log escapes the client's method and tool name (issue #435)
    add_executable(test_mcp_log_escaping
        tests/unit/test_mcp_log_escaping.cpp
        src/mcp/McpServer.cpp
        src/mcp/McpHttpParser.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_log_escaping PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_log_escaping COMMAND test_mcp_log_escaping)

    # MCP requests arriving during a tool's nested event loop (issue #254)
    add_executable(test_mcp_server_reentry
        tests/unit/test_mcp_server_reentry.cpp
        src/mcp/McpServer.cpp
        src/mcp/McpHttpParser.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_server_reentry PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_server_reentry COMMAND test_mcp_server_reentry)

    # MCP tools/call with an unknown tool name is a -32602 error (issue #428)
    add_executable(test_mcp_unknown_tool
        tests/unit/test_mcp_unknown_tool.cpp
        src/mcp/McpServer.cpp
        src/mcp/McpHttpParser.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_unknown_tool PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_unknown_tool COMMAND test_mcp_unknown_tool)

    # Agent panel script generation handlers (issue #260). Built without the
    # agent prompt resources, so the generator fails at once instead of
    # sending a request to a provider.
    set(AGENT_SOURCES_NO_QRC ${AGENT_SOURCES})
    list(FILTER AGENT_SOURCES_NO_QRC EXCLUDE REGEX "\\.qrc$")
    add_executable(test_agent_generation_handlers
        tests/unit/test_agent_generation_handlers.cpp
        ${UI_SOURCES}
        ${AGENT_SOURCES_NO_QRC}
    )
    target_link_libraries(test_agent_generation_handlers PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_agent_generation_handlers PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_agent_generation_handlers PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_agent_generation_handlers COMMAND test_agent_generation_handlers)

    # Agent panel Clear while a script runs (issue #261)
    add_executable(test_agent_clear
        tests/unit/test_agent_clear.cpp
        ${UI_SOURCES}
        ${AGENT_SOURCES_NO_QRC}
    )
    target_link_libraries(test_agent_clear PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_agent_clear PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_agent_clear PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_agent_clear COMMAND test_agent_clear)

    # Startup scripts of background tabs (real MainWindow, application sources)
    add_executable(test_startup_script_background
        tests/integration/test_startup_script_background.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_startup_script_background PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_startup_script_background PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_startup_script_background PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_startup_script_background COMMAND test_startup_script_background)

    # Startup script started while another script runs (issue #303)
    add_executable(test_startup_script_while_running
        tests/integration/test_startup_script_while_running.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_startup_script_while_running PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_startup_script_while_running PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_startup_script_while_running PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_startup_script_while_running COMMAND test_startup_script_while_running)

    # MCP write_file argument validation (McpToolHandler lives in the
    # application target).
    add_executable(test_mcp_write_file
        tests/unit/test_mcp_write_file.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_write_file PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_write_file COMMAND test_mcp_write_file)

    # "$" escaping in MCP tool scripts and recorded macros
    add_executable(test_script_dollar_escape
        tests/unit/test_script_dollar_escape.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_script_dollar_escape PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_script_dollar_escape COMMAND test_script_dollar_escape)

    # Script INPUT dialog (application target, uses QtUiStyle)
    add_executable(test_script_input_dialog
        tests/unit/test_script_input_dialog.cpp
        src/ui/dialogs/script_input_dialog.cpp
    )
    target_link_libraries(test_script_input_dialog PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_script_input_dialog COMMAND test_script_input_dialog)

    # Application log viewer (application target, uses QtUiStyle)
    add_executable(test_log_viewer
        tests/unit/test_log_viewer.cpp
        src/ui/dialogs/log_viewer.cpp
    )
    target_link_libraries(test_log_viewer PRIVATE tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_log_viewer COMMAND test_log_viewer)

    # Session > View Session Logs shows the tab's own log (issue #352)
    add_executable(test_session_log_view
        tests/unit/test_session_log_view.cpp
        src/ui/dialogs/log_viewer.cpp
    )
    target_link_libraries(test_session_log_view PRIVATE tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_session_log_view COMMAND test_session_log_view)

    # Script saves must not commit a truncated file
    add_executable(test_script_save_flush
        tests/unit/test_script_save_flush.cpp
        src/ui/dialogs/script_editor_window.cpp
        src/ui/dialogs/script_editor_dialog.cpp
    )
    target_link_libraries(test_script_save_flush PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_script_save_flush COMMAND test_script_save_flush)

    # Script editor window (application target, uses QtUiStyle)
    add_executable(test_script_editor_window
        tests/unit/test_script_editor_window.cpp
        src/ui/dialogs/script_editor_window.cpp
        src/ui/dialogs/script_editor_dialog.cpp
    )
    target_link_libraries(test_script_editor_window PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_script_editor_window COMMAND test_script_editor_window)

    # Connect dialog session variables (application target, uses QtUiStyle)
    add_executable(test_connect_dialog_session_variables
        tests/unit/test_connect_dialog_session_variables.cpp
        src/ui/dialogs/connect_dialog.cpp
        src/ui/dialogs/script_editor_window.cpp
        src/ui/dialogs/script_editor_dialog.cpp
        src/ui/themes/terminal_theme_manager.cpp
    )
    target_link_libraries(test_connect_dialog_session_variables PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_connect_dialog_session_variables
             COMMAND test_connect_dialog_session_variables)
    add_executable(test_connect_dialog_delete_session
        tests/unit/test_connect_dialog_delete_session.cpp
        src/ui/dialogs/connect_dialog.cpp
        src/ui/dialogs/script_editor_window.cpp
        src/ui/dialogs/script_editor_dialog.cpp
        src/ui/themes/terminal_theme_manager.cpp
    )
    target_link_libraries(test_connect_dialog_delete_session PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_connect_dialog_delete_session
             COMMAND test_connect_dialog_delete_session)
    add_executable(test_connect_dialog_tls_fields
        tests/unit/test_connect_dialog_tls_fields.cpp
        src/ui/dialogs/connect_dialog.cpp
        src/ui/dialogs/script_editor_window.cpp
        src/ui/dialogs/script_editor_dialog.cpp
        src/ui/themes/terminal_theme_manager.cpp
    )
    target_link_libraries(test_connect_dialog_tls_fields PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_connect_dialog_tls_fields
             COMMAND test_connect_dialog_tls_fields)
    add_executable(test_connect_dialog_device_size
        tests/unit/test_connect_dialog_device_size.cpp
        src/ui/dialogs/connect_dialog.cpp
        src/ui/dialogs/script_editor_window.cpp
        src/ui/dialogs/script_editor_dialog.cpp
        src/ui/themes/terminal_theme_manager.cpp
    )
    target_link_libraries(test_connect_dialog_device_size PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_connect_dialog_device_size
             COMMAND test_connect_dialog_device_size)

    # Match & Replace dialog refuses invalid regexes (issue #461)
    add_executable(test_match_replace_dialog
        tests/unit/test_match_replace_dialog.cpp
        src/ui/dialogs/match_replace_dialog.cpp)
    target_link_libraries(test_match_replace_dialog PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_match_replace_dialog COMMAND test_match_replace_dialog)

    # Quit confirmation with connected sessions (issue #409)
    add_executable(test_quit_confirmation tests/unit/test_quit_confirmation.cpp)
    target_link_libraries(test_quit_confirmation PRIVATE tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_quit_confirmation COMMAND test_quit_confirmation)

    # Main window geometry persistence (issue #412)
    add_executable(test_window_geometry tests/unit/test_window_geometry.cpp)
    target_link_libraries(test_window_geometry PRIVATE tn5250_core Qt6::Test Qt6::Widgets)
    add_test(NAME test_window_geometry COMMAND test_window_geometry)

    # MCP transport tests
    # McpHttpParser lives in the main application target (not tn5250_core),
    # so compile it into the test binary directly.
    add_executable(test_mcp_http_parser
        tests/unit/test_mcp_http_parser.cpp
        src/mcp/McpHttpParser.cpp)
    target_link_libraries(test_mcp_http_parser PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_mcp_http_parser COMMAND test_mcp_http_parser)
    add_executable(test_mcp_chunked_request
        tests/unit/test_mcp_chunked_request.cpp
        src/mcp/McpHttpParser.cpp)
    target_link_libraries(test_mcp_chunked_request PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_mcp_chunked_request COMMAND test_mcp_chunked_request)
    add_tn5250_test(test_mcp_validation tests/unit/test_mcp_validation.cpp)
    # MCP server refuses cross-origin and DNS-rebound requests
    add_executable(test_mcp_request_guard
        tests/unit/test_mcp_request_guard.cpp
        src/mcp/McpServer.cpp
        src/mcp/McpHttpParser.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_request_guard PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_request_guard COMMAND test_mcp_request_guard)

    # MCP JSON-RPC batches and non-object bodies (issue #427)
    add_executable(test_mcp_jsonrpc_batch
        tests/unit/test_mcp_jsonrpc_batch.cpp
        src/mcp/McpServer.cpp
        src/mcp/McpHttpParser.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_mcp_jsonrpc_batch PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_mcp_jsonrpc_batch COMMAND test_mcp_jsonrpc_batch)

    # Scripting tests
    # AI secrets in the settings file (AgentConfig lives in the application target)
    add_executable(test_secret_settings_permissions
        tests/unit/test_secret_settings_permissions.cpp
        src/agent/config.cpp
        src/agent/auth/token_storage.cpp)
    target_link_libraries(test_secret_settings_permissions PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_secret_settings_permissions COMMAND test_secret_settings_permissions)

    add_tn5250_test(test_script_lexer tests/unit/test_script_lexer.cpp)
    add_tn5250_test(test_script_escape tests/unit/test_script_escape.cpp)
    add_tn5250_test(test_script_timeout tests/unit/test_script_timeout.cpp)
    add_tn5250_test(test_script_parser tests/unit/test_script_parser.cpp)

    # OAuth redirect listener (AgentSources live in the application target)
    add_executable(test_oauth_callback
        tests/unit/test_oauth_callback.cpp
        src/agent/auth/oauth_flow.cpp)
    target_link_libraries(test_oauth_callback PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_oauth_callback COMMAND test_oauth_callback)

    # Integration tests
    add_tn5250_test(test_tn5250_connection tests/integration/test_tn5250_connection.cpp)
    add_tn5250_test(test_tn5250_tls_connection tests/integration/test_tn5250_tls_connection.cpp)
    add_tn5250_test(test_session_worker_reconnect tests/integration/test_session_worker_reconnect.cpp)
    # Session > Reconnect through the real MainWindow (application sources)
    add_executable(test_reconnect_teardown
        tests/integration/test_reconnect_teardown.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_reconnect_teardown PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_reconnect_teardown PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_reconnect_teardown PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_reconnect_teardown COMMAND test_reconnect_teardown)
    add_tn5250_test(test_session_worker_stop tests/integration/test_session_worker_stop.cpp)

    # Session > Save through the real MainWindow (application sources)
    add_executable(test_save_session_device_name
        tests/integration/test_save_session_device_name.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_save_session_device_name PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_save_session_device_name PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_save_session_device_name PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_save_session_device_name COMMAND test_save_session_device_name)

    # Theme change after closing a tab (stylesheet cache of the closed tab)
    add_executable(test_close_tab_theme_change
        tests/integration/test_close_tab_theme_change.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_close_tab_theme_change PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_close_tab_theme_change PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_close_tab_theme_change PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_close_tab_theme_change COMMAND test_close_tab_theme_change)

    # Remap Keyboard capture and virtual keyboard modifiers (issue #299)
    add_executable(test_remap_edge_cases
        tests/unit/test_remap_edge_cases.cpp
        src/ui/dialogs/keyboard_remap_dialog.cpp
        src/ui/widgets/QVirtualKeyboardWidget/QVirtualKeyboardWidget.cpp
    )
    target_link_libraries(test_remap_edge_cases PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_remap_edge_cases COMMAND test_remap_edge_cases)

    # Remap Keyboard warns about menu and window shortcuts (issue #371)
    add_executable(test_remap_menu_shortcut_warning
        tests/unit/test_remap_menu_shortcut_warning.cpp
        src/ui/dialogs/keyboard_remap_dialog.cpp
    )
    target_link_libraries(test_remap_menu_shortcut_warning PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_remap_menu_shortcut_warning COMMAND test_remap_menu_shortcut_warning)

    # Session > Save over a profile edited since the tab opened (issue #348)
    add_executable(test_save_session_profile_edits
        tests/integration/test_save_session_profile_edits.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_save_session_profile_edits PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_save_session_profile_edits PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_save_session_profile_edits PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_save_session_profile_edits COMMAND test_save_session_profile_edits)

    # Reconnect keeps the tab's place and label (issue #470)
    add_executable(test_reconnect_keeps_tab
        tests/integration/test_reconnect_keeps_tab.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_reconnect_keeps_tab PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_reconnect_keeps_tab PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_reconnect_keeps_tab PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_reconnect_keeps_tab COMMAND test_reconnect_keeps_tab)

    # Edited built-in terminal themes (issue #287)
    add_executable(test_edited_builtin_theme
        tests/unit/test_edited_builtin_theme.cpp
        src/ui/themes/terminal_theme_manager.cpp
        src/ui/themes/themes.qrc
    )
    target_link_libraries(test_edited_builtin_theme PRIVATE tn5250_core Qt6::Test)
    add_test(NAME test_edited_builtin_theme COMMAND test_edited_builtin_theme)

    # Settings 5250 Theme page Apply of built-in themes (issue #390)
    add_executable(test_theme_editor_apply
        tests/integration/test_theme_editor_apply.cpp
        resources/resources.qrc
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_theme_editor_apply PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_theme_editor_apply PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_theme_editor_apply PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_theme_editor_apply COMMAND test_theme_editor_apply)

    # Settings 5250 Theme page note on what Save does (issue #355)
    add_executable(test_theme_page_note
        tests/unit/test_theme_page_note.cpp
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_theme_page_note PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_theme_page_note PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_theme_page_note PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_theme_page_note COMMAND test_theme_page_note)

    # Session Settings preview stays in view while settings scroll (issue #365)
    add_executable(test_settings_preview_visible
        tests/unit/test_settings_preview_visible.cpp
        ${UI_SOURCES}
        ${AGENT_SOURCES}
    )
    target_link_libraries(test_settings_preview_visible PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_settings_preview_visible PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_settings_preview_visible PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_settings_preview_visible COMMAND test_settings_preview_visible)

    # Agent panel tool escaping, arrow keys and file I/O (issue #262)
    add_executable(test_agent_tool_escaping
        tests/unit/test_agent_tool_escaping.cpp
        ${UI_SOURCES}
        ${AGENT_SOURCES_NO_QRC}
    )
    target_link_libraries(test_agent_tool_escaping PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_agent_tool_escaping PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_agent_tool_escaping PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_agent_tool_escaping COMMAND test_agent_tool_escaping)

    # Providers and a tool call cut off by max_tokens (issue #259)
    add_executable(test_provider_truncated_tool_call
        tests/unit/test_provider_truncated_tool_call.cpp
        ${UI_SOURCES}
        ${AGENT_SOURCES_NO_QRC}
    )
    target_link_libraries(test_provider_truncated_tool_call PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    target_include_directories(test_provider_truncated_tool_call PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    if(HAVE_QT6_SSL)
        target_compile_definitions(test_provider_truncated_tool_call PRIVATE HAVE_QT6_SSL)
    endif()
    add_test(NAME test_provider_truncated_tool_call COMMAND test_provider_truncated_tool_call)

    # move_cursor row/column bounds (issue #402)
    add_executable(test_move_cursor_bounds
        tests/unit/test_move_cursor_bounds.cpp
        src/mcp/McpToolHandler.cpp
        src/mcp/McpSessionRegistry.cpp
        src/agent/agent_script_runner.cpp
        src/ui/dialogs/mcp_password_prompt_dialog.cpp
    )
    target_link_libraries(test_move_cursor_bounds PRIVATE
        tn5250_core Qt6::Test QtUiStyle::Dialogs)
    add_test(NAME test_move_cursor_bounds COMMAND test_move_cursor_bounds)

    # Command-line validation: each invalid invocation must exit with an
    # error on stderr instead of starting the GUI. A GUI app on Windows has
    # no stderr for CTest to read, so these run elsewhere only.
    if(NOT WIN32)
        macro(add_cli_error_test NAME EXPECTED)
            add_test(NAME test_cli_${NAME} COMMAND 5250ng ${ARGN})
            set_tests_properties(test_cli_${NAME} PROPERTIES
                PASS_REGULAR_EXPRESSION "${EXPECTED}"
                TIMEOUT 30
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_BINARY_DIR}/cli-test-home;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/cli-test-home/.config")
        endmacro()
        add_cli_error_test(orphan_port "--port requires --host" -p 99999)
        add_cli_error_test(orphan_tls "--tls requires --host" --tls)
        add_cli_error_test(orphan_mcp_port "--mcp-server-port requires --enable-mcp-server"
                           --mcp-server-port 80)
        add_cli_error_test(bad_mcp_port "Invalid MCP port number: 80"
                           --enable-mcp-server --mcp-server-port 80)
        add_cli_error_test(bad_port "Invalid port number: 99999" --host 127.0.0.1 -p 99999)
        add_cli_error_test(positional "Unexpected argument: extra" extra)
    endif()

    message(STATUS "Testing enabled - unit and integration tests configured")
endif()

# Installation
install(TARGETS 5250ng
    BUNDLE DESTINATION .
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
        MACOSX_BUNDLE_ICON_FILE "5250ng.icns"
        MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/Info.plist.in"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()
