cmake_minimum_required(VERSION 3.16)
project(5250ng VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_AI_MODULE "Build AI integration module" OFF)

# Qt6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Network
)

# SSL is optional (for TLS support)
# Try to find Qt6 SSL component first
find_package(Qt6 QUIET COMPONENTS Ssl)
if(Qt6Ssl_FOUND)
    message(STATUS "Qt6 SSL found - TLS support enabled")
    set(HAVE_QT6_SSL ON)
else()
    # Check if SSL is available through Network module (some Qt6 distributions include it)
    # Check if QSslSocket header file exists
    find_path(QT6_SSL_HEADER_DIR QSslSocket
        PATHS ${Qt6Network_INCLUDE_DIRS}
        PATH_SUFFIXES QtNetwork
        NO_DEFAULT_PATH
    )
    if(QT6_SSL_HEADER_DIR)
        message(STATUS "Qt6 SSL support found via Network module - TLS support enabled")
        set(HAVE_QT6_SSL ON)
        # SSL is part of Network module, no separate component needed
    else()
        message(STATUS "Qt6 SSL not found - TLS support disabled")
        message(STATUS "  Install libssl-dev and rebuild to enable TLS support")
        set(HAVE_QT6_SSL OFF)
    endif()
endif()

# Source files organized by layer
set(CORE_SOURCES
    src/core/ebcdic.cpp
    src/core/ebcdic.h
    src/core/keyboard_encoder.cpp
    src/core/keyboard_encoder.h
    src/core/input_handler.cpp
    src/core/input_handler.h
    src/core/session_config.cpp
    src/core/session_config.h
    src/core/session_manager.cpp
    src/core/session_manager.h
    src/core/logger.cpp
    src/core/logger.h
)

set(TRANSPORT_SOURCES
    src/transport/tn5250_client.cpp
    src/transport/tn5250_client.h
    src/transport/protocol_parser.cpp
    src/transport/protocol_parser.h
    src/transport/telnet_options.h
)

set(DISPLAY_SOURCES
    src/display/screen_buffer.cpp
    src/display/screen_buffer.h
    src/display/tn5250_widget.cpp
    src/display/tn5250_widget.h
)

set(UI_SOURCES
    src/ui/main_window.cpp
    src/ui/main_window.h
    src/ui/connect_dialog.cpp
    src/ui/connect_dialog.h
)

set(AI_SOURCES
    src/ai/
)

# Main executable
add_executable(tn5250
    src/main.cpp
    ${UI_SOURCES}
    ${TRANSPORT_SOURCES}
    ${DISPLAY_SOURCES}
    ${CORE_SOURCES}
)

target_link_libraries(tn5250
    PRIVATE 
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
)

if(HAVE_QT6_SSL)
    # If Qt6::Ssl component exists, link it; otherwise SSL is in Network module
    if(TARGET Qt6::Ssl)
        target_link_libraries(tn5250 PRIVATE Qt6::Ssl)
    endif
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
    
    # Unit tests
    add_executable(test_protocol_parser
        tests/unit/test_protocol_parser.cpp
        ${TRANSPORT_SOURCES}
        src/core/logger.cpp
        src/core/logger.h
    )
    target_link_libraries(test_protocol_parser
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Network
    )
    target_include_directories(test_protocol_parser PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME ProtocolParser COMMAND test_protocol_parser)
    
    # EBCDIC conversion tests
    add_executable(test_ebcdic
        tests/unit/test_ebcdic.cpp
        src/core/ebcdic.cpp
        src/core/ebcdic.h
    )
    target_link_libraries(test_ebcdic
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    target_include_directories(test_ebcdic PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME EBCDIC COMMAND test_ebcdic)
    
    # Screen buffer tests
    add_executable(test_screen_buffer
        tests/unit/test_screen_buffer.cpp
        ${DISPLAY_SOURCES}
        ${CORE_SOURCES}
    )
    target_link_libraries(test_screen_buffer
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Widgets
    )
    target_include_directories(test_screen_buffer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME ScreenBuffer COMMAND test_screen_buffer)
    
    # Keyboard encoder tests
    add_executable(test_keyboard_encoder
        tests/unit/test_keyboard_encoder.cpp
        src/core/keyboard_encoder.cpp
        src/core/keyboard_encoder.h
        src/core/ebcdic.cpp
        src/core/ebcdic.h
    )
    target_link_libraries(test_keyboard_encoder
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Widgets
    )
    target_include_directories(test_keyboard_encoder PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME KeyboardEncoder COMMAND test_keyboard_encoder)
    
    # Input handler tests
    add_executable(test_input_handler
        tests/unit/test_input_handler.cpp
        ${CORE_SOURCES}
        ${DISPLAY_SOURCES}
    )
    target_link_libraries(test_input_handler
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Widgets
    )
    target_include_directories(test_input_handler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME InputHandler COMMAND test_input_handler)
    
    # Session config tests
    add_executable(test_session_config
        tests/unit/test_session_config.cpp
        src/core/session_config.cpp
        src/core/session_config.h
    )
    target_link_libraries(test_session_config
        PRIVATE
            Qt6::Core
            Qt6::Test
    )
    target_include_directories(test_session_config PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME SessionConfig COMMAND test_session_config)
    
    # Integration tests
    add_executable(test_tn5250_connection
        tests/integration/test_tn5250_connection.cpp
        ${TRANSPORT_SOURCES}
        src/core/logger.cpp
        src/core/logger.h
    )
    target_link_libraries(test_tn5250_connection
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::Network
    )
    if(HAVE_QT6_SSL)
        target_link_libraries(test_tn5250_connection PRIVATE Qt6::Ssl)
        target_compile_definitions(test_tn5250_connection PRIVATE HAVE_QT6_SSL)
    endif()
    target_include_directories(test_tn5250_connection PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME TN5250Connection COMMAND test_tn5250_connection)
    
    message(STATUS "Testing enabled - unit and integration tests configured")
endif()

# AI module (optional, disabled by default)
if(BUILD_AI_MODULE)
    message(STATUS "AI module enabled")
    # AI module will be added in Phase 6
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
