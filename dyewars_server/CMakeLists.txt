cmake_minimum_required(VERSION 3.31)
project(DyeWarsServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        src/network/packets/Sender.h "src/server/ClientManager.h" "src/server/ClientManager.cpp" "src/network/ConnectionLimiter.h" "src/network/ConnectionLimiter.cpp"
        src/game/PlayerRegistry.cpp
        src/game/PlayerRegistry.h
        src/game/World.cpp
        src/game/World.h
        src/network/packets/incoming/PacketHandler.cpp
        src/network/packets/incoming/PacketHandler.h
        src/network/packets/outgoing/PacketSender.h
        src/game/Actions.h
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