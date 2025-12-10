#pragma once

#include <sol/sol.hpp>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <filesystem>

class LuaGameEngine {
public:
    LuaGameEngine();

    ~LuaGameEngine();

    void OnPlayerMoved(uint64_t player_id, int x, int y, uint8_t facing);

    bool ProcessMove(int &x, int &y, uint8_t direction);

    std::vector<uint8_t> ProcessCustomMessage(const std::vector<uint8_t> &data);

    void ReloadScripts();

private:
    void SetupLuaEnvironment();

    void CreateDefaultScript();

    void LoadScript(const std::string &path);

    void StartFileWatcher();

    sol::state lua_;
    std::mutex lua_mutex_;
    std::thread file_watcher_thread_;
    std::atomic<bool> stop_watching_{false};

    std::string active_script_path_ = "../scripts/main.lua";
};