// DatabaseManager.h
// Simple SQLite database manager for player accounts
//
// Install SQLite: https://www.sqlite.org/download.html
// Link with: -lsqlite3
//
// On Windows with vcpkg: vcpkg install sqlite3
// On Linux: sudo apt-get install libsqlite3-dev

#pragma once

#include <string>
#include <optional>
#include <vector>
#include <sqlite3.h>
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>

struct PlayerAccount {
    uint64_t user_id;
    std::string username;
    std::string password_hash;
    int level;
    int experience;
    int gold;
    int health;
    int mana;
    int x, y;
    int map_id;
    int last_x, last_y;
};

struct InventorySlot {
    int slot;
    int item_id;
    int quantity;
};

struct MailMessage {
    int id;
    uint64_t sender_id;
    std::string sender_name;
    std::string subject;
    std::string body;
    int gold;
    int item_id;
    int item_quantity;
    bool read;
    int64_t sent_at;
};


class DatabaseManager {
public:
    DatabaseManager(const std::string &db_path = "data/gameDB.sqlite") {
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
            db_ = nullptr;
            return;
        }

        // Enable WAL mode for better concurrency
        Execute("PRAGMA journal_mode=WAL;");

        std::cout << "Database opened: " << db_path << std::endl;
        CreateTables();
        StartWriteQueue();
    }

    ~DatabaseManager() {
        StopWriteQueue();
        if (db_) sqlite3_close(db_);
    }

    // ===== Player Account =====
    std::optional<PlayerAccount> GetPlayer(const std::string &username);

    std::optional<PlayerAccount> GetPlayer(uint32_t user_id);

    std::optional<PlayerAccount> CreatePlayer(const std::string &username, const std::string &password_hash);

    bool ValidatePassword(const std::string &username, const std::string &password_hash);

    // ===== Async Writes (queued) =====
    void SavePlayerStats(uint32_t user_id, int level, int exp, int gold, int health, int mana);

    void SavePlayerPosition(uint32_t user_id, int x, int y, int map_id);

    // ===== Inventory =====
    std::vector<InventorySlot> GetInventory(uint32_t user_id);

    void SetInventorySlot(uint32_t user_id, int slot, int item_id, int quantity);

    void ClearInventorySlot(uint32_t user_id, int slot);

    // ===== Spells =====
    std::vector<int> GetPlayerSpells(uint32_t user_id);

    void LearnSpell(uint32_t user_id, int spell_id);

    void ForgetSpell(uint32_t user_id, int spell_id);

    // ===== Mail =====
    std::vector<MailMessage> GetMail(uint32_t user_id, bool unread_only = false);

    void SendMail(uint32_t sender_id, uint32_t recipient_id, const std::string &subject,
                  const std::string &body, int gold = 0, int item_id = 0, int item_qty = 0);

    void MarkMailRead(int mail_id);

    void DeleteMail(int mail_id);

    // ===== Leaderboard =====
    std::vector<PlayerAccount> GetLeaderboard(int limit = 10);

private:
    void CreateTables();

    bool Execute(const char *sql);

    void EnqueueWrite(std::function<void()> write_fn);

    void StartWriteQueue();

    void StopWriteQueue();

    void ProcessWriteQueue();

    sqlite3 *db_ = nullptr;

    // Write queue
    std::queue<std::function<void()>> write_queue_;
    std::mutex queue_mutex_;
    std::thread write_thread_;
    std::atomic<bool> stop_queue_{false};
    std::condition_variable queue_cv_;
};

// Example usage in your server:
/*
// In GameServer constructor:
db_manager_ = std::make_unique<DatabaseManager>("game_server.db");

// When player connects with packet 0x05 (login):
case 0x05: // Login packet
    {
        // Extract username from packet
        std::string username(data.begin() + 1, data.end());

        auto account = db_manager_->LoginOrRegister(username);

        if (account) {
            player_user_id_ = account->user_id;
            player_name_ = account->username;

            // Send success packet with user_id
            SendLoginSuccess(account->user_id, account->username);
        } else {
            // Send login failure
            SendLoginFailure();
        }
    }
    break;

// Periodically save player position:
db_manager_->SavePlayerPosition(player_user_id_, player_x_, player_y_);

// Update stats when player levels up:
db_manager_->UpdatePlayerStats(player_user_id_, new_level, new_exp);
*/