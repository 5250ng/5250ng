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


# Source files organized by layer
set(CORE_SOURCES
    src/core/ebcdic.cpp
    src/core/ebcdic.h
    src/core/codepage.cpp
    src/core/codepage.h
    src/core/keyboard_encoder.cpp
    src/core/keyboard_encoder.h
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

set(TN5250_CLIENT_SOURCES
    src/network/tn5250/client/client_connection.cpp
    src/network/tn5250/client/client_io.cpp
    src/network/tn5250/client/client_telnet.cpp
    src/network/tn5250/client/client_handshake.cpp
    src/network/tn5250/client/client_tls.cpp
    src/network/tn5250/client/ibmrseed.cpp
    src/network/tn5250/client/ibmrseed.h
    src/network/tn5250/client/client.h
    src/network/tn5250/client/decoder.h
    src/network/tn5250/client/decoder.cpp
    # message
    src/network/tn5250/message/message.cpp
    src/network/tn5250/message/message.h
    src/network/tn5250/message/header/header.cpp
    src/network/tn5250/message/header/header.h
    src/network/tn5250/message/header/record_type.cpp
    src/network/tn5250/message/header/record_type.h
    src/network/tn5250/message/operation_code.cpp
    src/network/tn5250/message/operation_code.h
    # command
    src/network/tn5250/message/command/command.cpp
    src/network/tn5250/message/command/command.h
    src/network/tn5250/message/command/command_base.h
    src/network/tn5250/message/command/commands/command_cs_clear_screen.cpp
    src/network/tn5250/message/command/commands/command_cs_clear_screen.h
    src/network/tn5250/message/command/commands/command_rmf_read_mdt_fields.cpp
    src/network/tn5250/message/command/commands/command_rmf_read_mdt_fields.h
    src/network/tn5250/message/command/commands/command_wtd_write_to_display.cpp
    src/network/tn5250/message/command/commands/command_wtd_write_to_display.h
    src/network/tn5250/message/command/command_code.cpp
    src/network/tn5250/message/command/command_code.h
    # order base
    src/network/tn5250/message/command/order/order.h
    src/network/tn5250/message/command/order/order.cpp
    src/network/tn5250/message/command/order/order_base.h
    src/network/tn5250/message/command/order/order_code.cpp
    src/network/tn5250/message/command/order/order_code.h
    # orders
    src/network/tn5250/message/command/order/orders/order_ic_insert_cursor.cpp
    src/network/tn5250/message/command/order/orders/order_ic_insert_cursor.h
    src/network/tn5250/message/command/order/orders/order_ra_repeat_to_address.cpp
    src/network/tn5250/message/command/order/orders/order_ra_repeat_to_address.h
    src/network/tn5250/message/command/order/orders/order_sba_set_buffer_address.cpp
    src/network/tn5250/message/command/order/orders/order_sba_set_buffer_address.h
    src/network/tn5250/message/command/order/orders/order_sf_start_field.cpp
    src/network/tn5250/message/command/order/orders/order_sf_start_field.h
    src/network/tn5250/message/command/order/orders/order_soh_start_of_header.cpp
    src/network/tn5250/message/command/order/orders/order_soh_start_of_header.h
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
    src/ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.cpp
    src/ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.h
    src/ui/widgets/Frameless/BaseFramelessWindow.cpp
    src/ui/widgets/Frameless/BaseFramelessWindow.h
    src/ui/widgets/Frameless/TitleBar.cpp
    src/ui/widgets/Frameless/TitleBar.h
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
    # setup
    src/ui/setup/setupUI.cpp
    src/ui/setup/setupMenuBar.cpp
    src/ui/setup/setupStatusBar.cpp
)

# Main executable
add_executable(tn5250
    src/main.cpp
    ${UI_SOURCES}
    ${TELNET_SOURCES}
    ${TN5250_CLIENT_SOURCES}
    ${SESSION_SOURCES}
    ${DISPLAY_SOURCES}
    ${CORE_SOURCES}
    ${LOGGER_SOURCES}
    ${UTILS_SOURCES}
)

target_link_libraries(tn5250
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        OpenSSL::Crypto
)

if(HAVE_QT6_SSL)
    target_link_libraries(tn5250 PRIVATE Qt6::Ssl)
    target_compile_definitions(tn5250 PRIVATE HAVE_QT6_SSL)
endif()

# Include directories
target_include_directories(tn5250 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set output directory
set_target_properties(tn5250 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Testing (when enabled)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # Shared static library — compiled once, linked by all test targets
    add_library(tn5250_core STATIC
        ${CORE_SOURCES}
        ${SESSION_SOURCES}
        ${TELNET_SOURCES}
        ${TN5250_CLIENT_SOURCES}
        ${LOGGER_SOURCES}
        ${UTILS_SOURCES}
        ${DISPLAY_SOURCES}
    )
    target_link_libraries(tn5250_core
        PUBLIC
            Qt6::Core
            Qt6::Network
            Qt6::Widgets
            OpenSSL::Crypto
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

    # Integration tests
    add_tn5250_test(test_tn5250_connection tests/integration/test_tn5250_connection.cpp)

    message(STATUS "Testing enabled - unit and integration tests configured")
endif()

# Installation (for future phases)
install(TARGETS tn5250
    RUNTIME DESTINATION bin
)

# Platform-specific settings
if(WIN32)
    set_target_properties(tn5250 PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(APPLE)
    set_target_properties(tn5250 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
endif()
