#include "include/lua/LuaEngine.h"
#include <iostream>
#include <fstream>

namespace fs = std::filesystem;

LuaGameEngine::LuaGameEngine() {
    SetupLuaEnvironment();
    CreateDefaultScript();
    LoadScript(active_script_path_);
    StartFileWatcher();
}

LuaGameEngine::~LuaGameEngine() {
    stop_watching_ = true;
    if (file_watcher_thread_.joinable()) {
        file_watcher_thread_.join();
    }
}

void LuaGameEngine::OnPlayerMoved(uint32_t player_id, int x, int y, uint8_t direction) {
    // 1. Lock the Mutex (Thread Safety)
    std::lock_guard<std::mutex> lock(lua_mutex_);

    try {
        // 2. Get the function from Lua
        sol::function on_move = lua_["on_player_moved"];

        // 3. Call it only if it exists
        if (on_move.valid()) {
            // Pass data to Lua
            // We use sol::protected_function_result to catch Lua errors without crashing C++
            auto result = on_move(player_id, x, y, direction);

            if (!result.valid()) {
                sol::error err = result;
                std::cout << "LUA ERROR (OnMove): " << err.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Lua Exception: " << e.what() << std::endl;
    }
}

bool LuaGameEngine::ProcessMove(int& x, int& y, uint8_t direction) {
    std::lock_guard<std::mutex> lock(lua_mutex_);
    try {
        sol::function move_func = lua_["process_move_command"];
        if (!move_func.valid()) return false;

        sol::object result = move_func(x, y, direction);
        if (result.is<sol::table>()) {
            sol::table result_table = result.as<sol::table>();
            if (result_table.size() >= 2) {
                x = result_table[1];
                y = result_table[2];
                return true;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "LUA ERROR: " << e.what() << std::endl;
    }
    return false;
}

std::vector<uint8_t> LuaGameEngine::ProcessCustomMessage(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(lua_mutex_);
    try {
        sol::function custom_func = lua_["process_custom_message"];
        if (!custom_func.valid()) return {};

        sol::table lua_data = lua_.create_table();
        for (size_t i = 0; i < data.size(); ++i) lua_data[i + 1] = data[i];

        sol::object result = custom_func(lua_data);
        if (result.is<sol::table>()) {
            sol::table result_table = result.as<sol::table>();
            std::vector<uint8_t> response;
            for (size_t i = 1; i <= result_table.size(); ++i) {
                response.push_back(result_table[i]);
            }
            return response;
        }
    } catch (const std::exception& e) {
        std::cout << "LUA ERROR: " << e.what() << std::endl;
    }
    return {};
}

void LuaGameEngine::ReloadScripts() {
    std::lock_guard<std::mutex> lock(lua_mutex_);
    std::cout << "\n=== Hot-reloading Lua scripts ===" << std::endl;
    try {
        lua_.collect_garbage();
        SetupLuaEnvironment();
        LoadScript(active_script_path_);
        std::cout << "Scripts reloaded successfully!\n" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Failed to reload: " << e.what() << "\n" << std::endl;
    }
}

void LuaGameEngine::SetupLuaEnvironment() {
    lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math);

    lua_.set_function("log", [this](sol::variadic_args args) {
        std::cout << "[LUA] ";
        for (auto arg : args) {
            // "tostring" is a Lua function that converts ANYTHING to text safely
            std::string str = lua_["tostring"](arg);
            std::cout << str << " ";
        }
        std::cout << std::endl;
    });
}

void LuaGameEngine::CreateDefaultScript() {
    fs::create_directories("game_scripts");
    if (fs::exists("game_scripts/main.lua")) return;

    std::ofstream file("game_scripts/main.lua");
    file << R"(
function process_move_command(x, y, direction)
    log(string.format("Move: (%d,%d) dir:%d", x, y, direction))
    local new_x, new_y = x, y
    if direction == DIRECTION_UP and y > 0 then new_y = y - 1
    elseif direction == DIRECTION_RIGHT and x < GRID_WIDTH - 1 then new_x = x + 1
    elseif direction == DIRECTION_DOWN and y < GRID_HEIGHT - 1 then new_y = y + 1
    elseif direction == DIRECTION_LEFT and x > 0 then new_x = x - 1 end
    return {new_x, new_y}
end
function process_custom_message(data) return data end
log("Game script loaded!")
)";
    file.close();
}

void LuaGameEngine::LoadScript(const std::string& filename) {
    // 1. Search for the file (Current Dir -> Up 1 -> Up 2 -> Up 3)
    std::string search_paths[] = {
            filename,
            "../" + filename,
            "../../" + filename,
            "../../../" + filename
    };

    std::string found_path = "";

    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            found_path = path;
            break;
        }
    }

    if (found_path.empty()) {
        std::cerr << "[Error] Could not find script: " << filename << std::endl;
        return;
    }

    // 2. SAVE THE PATH so the watcher knows where to look!
    active_script_path_ = found_path;

    // 3. Load it
    std::cout << "[Lua] Loading script from: " << fs::absolute(found_path) << std::endl;
    try {
        auto result = lua_.script_file(found_path, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            std::cout << "Script error: " << err.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}

void LuaGameEngine::StartFileWatcher() {
    file_watcher_thread_ = std::thread([this]() {
        // Wait for LoadScript to run at least once so we have a path
        while (active_script_path_.empty() && !stop_watching_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (stop_watching_) return;

        // Get initial timestamp of the FILE (not the folder)
        auto last_time = fs::last_write_time(active_script_path_);
        std::cout << "[Watcher] Watching file: " << active_script_path_ << std::endl;

        while (!stop_watching_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            try {
                auto current_time = fs::last_write_time(active_script_path_);
                if (current_time != last_time) {
                    last_time = current_time;
                    ReloadScripts();
                }
            } catch (...) {}
        }
    });
}