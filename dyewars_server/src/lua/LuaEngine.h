/// =======================================
/// DyeWarsServer - LuaGameEngine
///
/// Lua scripting bridge for game logic.
/// Allows hot-reloading scripts without server restart.
///
/// THREAD SAFETY:
/// --------------
/// This class is accessed from multiple threads:
///   - Game thread: OnPlayerMoved, ProcessMove, ProcessCustomMessage
///   - File watcher thread: monitors script changes for hot reload
///   - Console thread: ReloadScripts (from 'r' command)
///
/// SOLUTION: All Lua state access is protected by lua_mutex_.
///
/// WHY MUTEX FOR LUA:
/// Lua states are NOT thread-safe. Even read-only operations can crash
/// if another thread is modifying tables or running garbage collection.
/// sol2 provides no built-in thread safety, so we use explicit locking.
///
/// PATTERN USED:
/// Every public method that touches lua_ acquires lua_mutex_ first:
///   std::lock_guard<std::mutex> lock(lua_mutex_);
///   // Now safe to use lua_
///
/// FILE WATCHER NOTES:
/// The file watcher runs in its own thread and only:
///   - Reads active_script_path_ (set once during init, then immutable)
///   - Calls ReloadScripts() which acquires its own lock
/// This is safe because:
///   - active_script_path_ doesn't change after LoadScript() completes
///   - ReloadScripts() protects all Lua access with lua_mutex_
///
/// =======================================
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

    /// Called when a player moves - triggers Lua on_player_moved event.
    /// Thread-safe: acquires lua_mutex_.
    void OnPlayerMoved(uint64_t player_id, int x, int y, uint8_t facing);

    /// Process a move command through Lua.
    /// Thread-safe: acquires lua_mutex_.
    bool ProcessMove(int &x, int &y, uint8_t direction);

    /// Process a custom message through Lua.
    /// Thread-safe: acquires lua_mutex_.
    std::vector<uint8_t> ProcessCustomMessage(const std::vector<uint8_t> &data);

    /// Reload all Lua scripts (hot reload).
    /// Thread-safe: acquires lua_mutex_.
    /// Can be called from console ('r' command) or file watcher.
    void ReloadScripts();

private:
    void SetupLuaEnvironment();

    void CreateDefaultScript();

    void LoadScript(const std::string &path);

    void StartFileWatcher();

    // =========================================================================
    // DATA
    // =========================================================================

    /// The Lua state - ALL access must be protected by lua_mutex_!
    /// Lua states are NOT thread-safe.
    sol::state lua_;

    /// Mutex protecting all Lua state access.
    /// Must be held when:
    ///   - Calling any Lua function
    ///   - Reading/writing Lua tables
    ///   - Running garbage collection
    ///   - Loading/reloading scripts
    std::mutex lua_mutex_;

    /// File watcher thread for hot-reload functionality.
    std::thread file_watcher_thread_;

    /// Signal to stop the file watcher on shutdown.
    std::atomic<bool> stop_watching_{false};

    /// Path to the currently active script.
    /// Set once during initialization, then effectively immutable.
    /// The file watcher reads this without locking (safe because immutable).
    std::string active_script_path_ = "../scripts/main.lua";
};