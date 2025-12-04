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
#include <sqlite3.h>
#include <iostream>
#include <memory>

struct PlayerAccount {
    uint32_t user_id;
    std::string username;
    int level;
    int experience;
};

class DatabaseManager {
public:
    DatabaseManager(const std::string& db_path = "game_server.db") {
        int rc = sqlite3_open(db_path.c_str(), &db_);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
            db_ = nullptr;
            return;
        }

        std::cout << "Database opened: " << db_path << std::endl;
        CreateTables();
    }

    ~DatabaseManager() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    // Login/register - returns user_id
    std::optional<PlayerAccount> LoginOrRegister(const std::string& username) {
        if (!db_) return std::nullopt;

        // Try to find existing player
        auto existing = GetPlayerByUsername(username);
        if (existing) {
            std::cout << "Player '" << username << "' logged in (ID: " << existing->user_id << ")" << std::endl;
            return existing;
        }

        // Create new player
        return CreateNewPlayer(username);
    }

    // Get player by username
    std::optional<PlayerAccount> GetPlayerByUsername(const std::string& username) {
        if (!db_) return std::nullopt;

        const char* sql = "SELECT user_id, username, level, experience FROM players WHERE username = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
            return std::nullopt;
        }

        // Bind username parameter
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        std::optional<PlayerAccount> result;

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            PlayerAccount account;
            account.user_id = sqlite3_column_int(stmt, 0);
            account.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            account.level = sqlite3_column_int(stmt, 2);
            account.experience = sqlite3_column_int(stmt, 3);
            result = account;
        }

        sqlite3_finalize(stmt);
        return result;
    }

    // Get player by user_id
    std::optional<PlayerAccount> GetPlayerByID(uint32_t user_id) {
        if (!db_) return std::nullopt;

        const char* sql = "SELECT user_id, username, level, experience FROM players WHERE user_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
            return std::nullopt;
        }

        sqlite3_bind_int(stmt, 1, user_id);

        std::optional<PlayerAccount> result;

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            PlayerAccount account;
            account.user_id = sqlite3_column_int(stmt, 0);
            account.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            account.level = sqlite3_column_int(stmt, 2);
            account.experience = sqlite3_column_int(stmt, 3);
            result = account;
        }

        sqlite3_finalize(stmt);
        return result;
    }

    // Update player stats
    bool UpdatePlayerStats(uint32_t user_id, int level, int experience) {
        if (!db_) return false;

        const char* sql = "UPDATE players SET level = ?, experience = ? WHERE user_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, level);
        sqlite3_bind_int(stmt, 2, experience);
        sqlite3_bind_int(stmt, 3, user_id);

        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        if (success) {
            std::cout << "Updated player " << user_id << " stats" << std::endl;
        }

        return success;
    }

    // Save player position
    bool SavePlayerPosition(uint32_t user_id, int x, int y) {
        if (!db_) return false;

        const char* sql = "UPDATE players SET last_x = ?, last_y = ? WHERE user_id = ?;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, x);
        sqlite3_bind_int(stmt, 2, y);
        sqlite3_bind_int(stmt, 3, user_id);

        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        return success;
    }

private:
    void CreateTables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS players (
                user_id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                level INTEGER DEFAULT 1,
                experience INTEGER DEFAULT 0,
                last_x INTEGER DEFAULT 0,
                last_y INTEGER DEFAULT 0,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_login DATETIME DEFAULT CURRENT_TIMESTAMP
            );

            CREATE INDEX IF NOT EXISTS idx_username ON players(username);
        )";

        char* err_msg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::cerr << "SQL error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        } else {
            std::cout << "Database tables ready" << std::endl;
        }
    }

    std::optional<PlayerAccount> CreateNewPlayer(const std::string& username) {
        const char* sql = "INSERT INTO players (username) VALUES (?);";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db_) << std::endl;
            return std::nullopt;
        }

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Failed to create player: " << sqlite3_errmsg(db_) << std::endl;
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        uint32_t new_user_id = sqlite3_last_insert_rowid(db_);
        sqlite3_finalize(stmt);

        std::cout << "New player '" << username << "' created (ID: " << new_user_id << ")" << std::endl;

        // Return the newly created player
        PlayerAccount account;
        account.user_id = new_user_id;
        account.username = username;
        account.level = 1;
        account.experience = 0;

        return account;
    }

    sqlite3* db_;
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