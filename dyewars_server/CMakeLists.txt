cmake_minimum_required(VERSION 3.31)
project(DyeWarsServer)

set(CMAKE_CXX_STANDARD 20)

# Find packages
find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(lua REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(DyeWarsServer src/main.cpp
        include/database/DatabaseManager.h
        include/server/Common.h
        src/server/GameServer.cpp
        include/server/GameServer.h
        include/lua/LuaEngine.h
        src/lua/LuaEngine.cpp
        src/server/GameSession.cpp
        include/server/GameSession.h
        include/server/GameMap.h
        src/server/GameMap.cpp
        include/server/Player.h
        src/server/Player.cpp
        include/server/BandwidthMonitor.h)


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
        ${CMAKE_CURRENT_SOURCE_DIR}
)