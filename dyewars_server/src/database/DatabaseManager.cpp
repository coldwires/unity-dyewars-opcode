//
// Created by Anonymous on 12/6/2025.
//
// src/Database/DatabaseManager.cpp
#include "DatabaseManager.h"

void DatabaseManager::CreateTables() {
    const char *sql = R"(
        CREATE TABLE IF NOT EXISTS players (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            level INTEGER DEFAULT 1,
            experience INTEGER DEFAULT 0,
            gold INTEGER DEFAULT 0,
            health INTEGER DEFAULT 100,
            mana INTEGER DEFAULT 50,
            x INTEGER DEFAULT 0,
            y INTEGER DEFAULT 0,
            map_id INTEGER DEFAULT 1,
            created_at INTEGER DEFAULT (strftime('%s', 'now')),
            last_login INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE TABLE IF NOT EXISTS inventory (
            user_id INTEGER,
            slot INTEGER,
            item_id INTEGER,
            quantity INTEGER DEFAULT 1,
            PRIMARY KEY (user_id, slot)
        );

        CREATE TABLE IF NOT EXISTS player_spells (
            user_id INTEGER,
            spell_id INTEGER,
            PRIMARY KEY (user_id, spell_id)
        );

        CREATE TABLE IF NOT EXISTS mail (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender_id INTEGER,
            recipient_id INTEGER,
            subject TEXT,
            body TEXT,
            gold INTEGER DEFAULT 0,
            item_id INTEGER DEFAULT 0,
            item_quantity INTEGER DEFAULT 0,
            read INTEGER DEFAULT 0,
            sent_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE INDEX IF NOT EXISTS idx_players_username ON players(username);
        CREATE INDEX IF NOT EXISTS idx_inventory_user ON inventory(user_id);
        CREATE INDEX IF NOT EXISTS idx_mail_recipient ON mail(recipient_id, read);
    )";

    Execute(sql);
    std::cout << "Database tables ready" << std::endl;
}

bool DatabaseManager::Execute(const char *sql) {
    char *err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ===== Write Queue =====
void DatabaseManager::EnqueueWrite(std::function<void()> write_fn) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        write_queue_.push(std::move(write_fn));
    }
    queue_cv_.notify_one();
}

void DatabaseManager::StartWriteQueue() {
    write_thread_ = std::thread(&DatabaseManager::ProcessWriteQueue, this);
}

void DatabaseManager::StopWriteQueue() {
    stop_queue_ = true;
    queue_cv_.notify_one();
    if (write_thread_.joinable()) write_thread_.join();
}

void DatabaseManager::ProcessWriteQueue() {
    while (!stop_queue_) {
        std::function < void() > task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !write_queue_.empty() || stop_queue_; });

            if (stop_queue_ && write_queue_.empty()) break;

            task = std::move(write_queue_.front());
            write_queue_.pop();
        }
        if (task) task();
    }

    // Flush remaining writes on shutdown
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!write_queue_.empty()) {
        write_queue_.front()();
        write_queue_.pop();
    }
}

// ===== Player Operations =====
std::optional<PlayerAccount> DatabaseManager::GetPlayer(const std::string &username) {
    const char *sql = "SELECT user_id, username, password_hash, level, experience, gold, health, mana, x, y, map_id FROM players WHERE username = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<PlayerAccount> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        PlayerAccount p;
        p.user_id = sqlite3_column_int(stmt, 0);
        p.username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        p.password_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        p.level = sqlite3_column_int(stmt, 3);
        p.experience = sqlite3_column_int(stmt, 4);
        p.gold = sqlite3_column_int(stmt, 5);
        p.health = sqlite3_column_int(stmt, 6);
        p.mana = sqlite3_column_int(stmt, 7);
        p.x = sqlite3_column_int(stmt, 8);
        p.y = sqlite3_column_int(stmt, 9);
        p.map_id = sqlite3_column_int(stmt, 10);
        result = p;
    }
    sqlite3_finalize(stmt);
    return result;
}

void DatabaseManager::SavePlayerStats(uint32_t user_id, int level, int exp, int gold, int health, int mana) {
    EnqueueWrite([=, this]() {
        const char *sql = "UPDATE players SET level=?, experience=?, gold=?, health=?, mana=? WHERE user_id=?;";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, level);
            sqlite3_bind_int(stmt, 2, exp);
            sqlite3_bind_int(stmt, 3, gold);
            sqlite3_bind_int(stmt, 4, health);
            sqlite3_bind_int(stmt, 5, mana);
            sqlite3_bind_int(stmt, 6, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    });
}

// ... implement remaining methods similarly