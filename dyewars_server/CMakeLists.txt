if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
endif ()

cmake_minimum_required(VERSION 3.31)
project(DyeWarsServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Sanitizer Options (Linux/macOS only, GCC/Clang)
# Usage: cmake -B build -DENABLE_TSAN=ON
# =============================================================================
option(ENABLE_TSAN "Enable ThreadSanitizer for data race detection" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)

if(ENABLE_TSAN AND ENABLE_ASAN)
    message(FATAL_ERROR "TSAN and ASAN cannot be enabled simultaneously")
endif()

if(ENABLE_TSAN)
    if(MSVC)
        message(WARNING "ThreadSanitizer not supported on MSVC, ignoring ENABLE_TSAN")
    else()
        message(STATUS "ThreadSanitizer ENABLED - use for detecting data races")
        add_compile_options(-fsanitize=thread -g)
        add_link_options(-fsanitize=thread)
    endif()
endif()

if(ENABLE_ASAN)
    if(MSVC)
        message(WARNING "AddressSanitizer not fully supported on MSVC, ignoring ENABLE_ASAN")
    else()
        message(STATUS "AddressSanitizer ENABLED - use for detecting memory errors")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Find packages
find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(lua REQUIRED)
find_package(SQLite3 REQUIRED)

# Gather sources (exclude build directory)
file(GLOB_RECURSE SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/network/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/database/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/debug/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

file(GLOB_RECURSE HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/network/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/database/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/*.h"
)

add_executable(DyeWarsServer ${SOURCES} ${HEADERS}
        src/core/Log.h
        "src/server/ClientManager.h" "src/server/ClientManager.cpp" "src/network/ConnectionLimiter.h" "src/network/ConnectionLimiter.cpp"
        src/game/PlayerRegistry.h
        src/game/World.h
        src/network/packets/incoming/PacketHandler.cpp
        src/network/packets/incoming/PacketHandler.h
        src/network/packets/outgoing/PacketSender.h
        src/game/actions/Actions.h
        src/game/TileMap.h
        src/game/actions/Actions.cpp
        src/game/actions/MoveActions.cpp
        src/game/actions/BotStressTest.cpp
        src/database/DatabaseManager.cpp
        src/game/SpatialHash.h
)

# Link libraries
target_link_libraries(DyeWarsServer PRIVATE
        asio::asio
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        sol2::sol2
        SQLite::SQLite3
        ${LUA_LIBRARIES}
)

# Include directories - add your source directories
target_include_directories(DyeWarsServer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LUA_INCLUDE_DIR}
)

# =============================================================================
# Tests (optional)
# Usage: cmake -B build -DBUILD_TESTS=ON
# =============================================================================
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    message(STATUS "Building tests enabled")

    # Gather test sources (exclude main.cpp from server sources)
    file(GLOB_RECURSE TEST_SERVER_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/game/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/network/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/database/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lua/*.cpp"
    )

    add_executable(DyeWarsTests
            tests/test_cleanup.cpp
            ${TEST_SERVER_SOURCES}
    )

    target_link_libraries(DyeWarsTests PRIVATE
            asio::asio
            nlohmann_json::nlohmann_json
            spdlog::spdlog
            sol2::sol2
            SQLite::SQLite3
            ${LUA_LIBRARIES}
    )

    target_include_directories(DyeWarsTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${LUA_INCLUDE_DIR}
    )

    # Enable CTest
    enable_testing()
    add_test(NAME CleanupTests COMMAND DyeWarsTests)
endif()