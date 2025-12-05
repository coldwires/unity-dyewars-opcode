#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <chrono>
#include <string>

class ConnectionLimiter {
public:
    // Check if IP can connect (under max concurrent)
    bool CanConnect(const std::string& ip);

    // Track connection opened/closed
    void AddConnection(const std::string& ip);
    void RemoveConnection(const std::string& ip);

    // Rate limiting (attempts per time window)
    bool CheckRateLimit(const std::string& ip);

    // Ban management
    void RecordFailure(const std::string& ip);
    bool IsBanned(const std::string& ip);
    void Unban(const std::string& ip);

    // Stats
    int GetConnectionCount(const std::string& ip);
    int GetBanCount();

private:
    // Concurrent connections per IP
    std::unordered_map<std::string, int> connections_;
    static constexpr int MAX_CONNECTIONS_PER_IP = 5;

    // Rate limiting: attempts within time window
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> attempts_;
    static constexpr int MAX_ATTEMPTS_PER_WINDOW = 10;
    static constexpr auto RATE_WINDOW = std::chrono::seconds(60);

    // Ban list
    std::unordered_map<std::string, int> failures_;
    std::unordered_set<std::string> banned_;
    static constexpr int MAX_FAILURES_BEFORE_BAN = 5;

    std::mutex mutex_;
};