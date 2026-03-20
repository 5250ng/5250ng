cmake_minimum_required(VERSION 3.16)
project(5250ng VERSION 0.1.0 LANGUAGES CXX)

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

# Protocol library (standalone, Qt-free)
set(TN5250_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(src/network/tn5250/protocol-tn5250)

# Source files organized by layer
set(CORE_SOURCES
    src/core/ebcdic.cpp
    src/core/ebcdic.h
    src/core/codepage.cpp
    src/core/codepage.h
    src/core/keyboard_encoder.cpp
    src/core/keyboard_encoder.h
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
    src/core/scripting/script_token.h
    src/core/scripting/script_ast.h
    src/core/scripting/script_lexer.cpp
    src/core/scripting/script_lexer.h
    src/core/scripting/script_parser.cpp
    src/core/scripting/script_parser.h
    src/core/scripting/script_executor.cpp
    src/core/scripting/script_executor.h
    src/core/scripting/script_compiler.cpp
    src/core/scripting/script_compiler.h
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
    src/network/tn5250/telnet/commands.cpp
    src/network/tn5250/telnet/commands.h
    src/network/tn5250/telnet/options.cpp
    src/network/tn5250/telnet/options.h
    src/network/tn5250/telnet/client.cpp
    src/network/tn5250/telnet/client.h
    src/network/tn5250/devices/devices.cpp
    src/network/tn5250/devices/devices.h
)

set(TN5250_ADAPTER_SOURCES
    src/network/tn5250/client/client.h
    src/network/tn5250/client/client_connection.cpp
    src/network/tn5250/client/client_io.cpp
    src/network/tn5250/client/client_telnet.cpp
    src/network/tn5250/client/client_handshake.cpp
    src/network/tn5250/client/client_tls.cpp
    src/network/tn5250/client/ibmrseed.cpp
    src/network/tn5250/client/ibmrseed.h
    src/network/tn5250/client/decoder_adapter.h
    src/network/tn5250/client/decoder_adapter.cpp
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
    src/network/hostserver/ifs_client.cpp
    src/network/hostserver/ifs_client.h
    src/network/hostserver/ifs_transfer.cpp
    src/network/hostserver/ifs_transfer.h
)

set(AGENT_SOURCES
    src/agent/provider.h
    src/agent/config.h
    src/agent/config.cpp
    src/agent/providers/openai_provider.h
    src/agent/providers/openai_provider.cpp
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
    src/ui/widgets/Q5250ScreenWidget/screen_buffer.cpp
    src/ui/widgets/Q5250ScreenWidget/screen_buffer.h
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
    src/ui/widgets/Frameless/BaseFramelessWindow.cpp
    src/ui/widgets/Frameless/BaseFramelessWindow.h
    src/ui/widgets/Frameless/BaseFramelessDialog.cpp
    src/ui/widgets/Frameless/BaseFramelessDialog.h
    src/ui/widgets/Frameless/TitleBar.cpp
    src/ui/widgets/Frameless/TitleBar.h
    src/ui/widgets/Frameless/titlebar_icons.qrc
    src/ui/widgets/Frameless/StyledMessageBox.cpp
    src/ui/widgets/Frameless/StyledMessageBox.h
    # dialogs
    src/ui/dialogs/log_viewer.cpp
    src/ui/dialogs/log_viewer.h
    src/ui/dialogs/connect_dialog.cpp
    src/ui/dialogs/connect_dialog.h
    src/ui/dialogs/settings_dialog.cpp
    src/ui/dialogs/settings_dialog.h
    src/ui/dialogs/session_settings_dialog.cpp
    src/ui/dialogs/session_settings_dialog.h
    src/ui/dialogs/theme_preview_widget.cpp
    src/ui/dialogs/theme_preview_widget.h
    src/ui/dialogs/file_transfer_dialog.cpp
    src/ui/dialogs/file_transfer_dialog.h
    src/ui/dialogs/match_replace_dialog.cpp
    src/ui/dialogs/match_replace_dialog.h
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
    src/ui/actions/onToggleFullscreen.cpp
    src/ui/actions/onEditActions.cpp
    src/ui/actions/onHotspots.cpp
    src/ui/actions/onScripts.cpp
    src/ui/actions/onSessionLogging.cpp
    src/ui/actions/onScreenHistory.cpp
    src/ui/actions/onToggleAgentPanel.cpp
    src/ui/actions/onFileTransfer.cpp
    src/ui/actions/onMatchReplace.cpp
    # setup
    src/ui/setup/setupUI.cpp
    src/ui/setup/setupMenuBar.cpp
    src/ui/setup/setupStatusBar.cpp
)

# Main executable
add_executable(5250ng
    src/main.cpp
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
)

target_link_libraries(5250ng
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        OpenSSL::Crypto
        tn5250::tn5250
)

if(HAVE_QT6_SSL)
    target_link_libraries(5250ng PRIVATE Qt6::Ssl)
    target_compile_definitions(5250ng PRIVATE HAVE_QT6_SSL)
endif()

# Include directories
target_include_directories(5250ng PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
    )
    if(HAVE_QT6_SSL)
        target_link_libraries(tn5250_core PUBLIC Qt6::Ssl)
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
    add_tn5250_test(test_session_config tests/unit/test_session_config.cpp)

    add_tn5250_test(test_ibmrseed tests/unit/test_ibmrseed.cpp)
    add_tn5250_test(test_codepage tests/unit/test_codepage.cpp)

    add_tn5250_test(test_terminal_theme tests/unit/test_terminal_theme.cpp)

    # Host server tests
    add_tn5250_test(test_host_data_stream tests/unit/test_host_data_stream.cpp)
    add_tn5250_test(test_password_encrypt tests/unit/test_password_encrypt.cpp)

    # Scripting tests
    add_tn5250_test(test_script_lexer tests/unit/test_script_lexer.cpp)
    add_tn5250_test(test_script_parser tests/unit/test_script_parser.cpp)

    # Integration tests
    add_tn5250_test(test_tn5250_connection tests/integration/test_tn5250_connection.cpp)

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
