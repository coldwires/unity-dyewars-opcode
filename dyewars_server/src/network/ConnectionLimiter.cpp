#include "ConnectionLimiter.h"
#include "core/Log.h"

bool ConnectionLimiter::CanConnect(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check ban first
    if (banned_.contains(ip))
        return false;

    // Check concurrent limit
    auto it = connections_.find(ip);
    if (it != connections_.end() && it->second >= MAX_CONNECTIONS_PER_IP)
        return false;

    return true;
}

void ConnectionLimiter::AddConnection(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[ip]++;
}

void ConnectionLimiter::RemoveConnection(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(ip);
    if (it == connections_.end()) {
        return;
    }
    if (--it->second <= 0) {
        connections_.erase(it);
    }
}

bool ConnectionLimiter::CheckRateLimit(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto &times = attempts_[ip];

    // Remove attempts outside window
    std::erase_if(times, [&](const auto &t) {
        return now - t > RATE_WINDOW;
    });

    // Check limit
    if (times.size() >= MAX_ATTEMPTS_PER_WINDOW)
        return false;

    // Record this attempt
    times.push_back(now);
    return true;
}

void ConnectionLimiter::RecordFailure(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (++failures_[ip] >= MAX_FAILURES_BEFORE_BAN) {
        banned_.insert(ip);
        Log::Warn("Auto-banned IP: {} after {} failures", ip, failures_[ip]);
    }
}

bool ConnectionLimiter::IsBanned(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    return banned_.contains(ip);
}

void ConnectionLimiter::Unban(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    banned_.erase(ip);
    failures_.erase(ip);
    Log::Info("Unbanned IP: {}", ip);
}

int ConnectionLimiter::GetConnectionCount(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(ip);
    return it != connections_.end() ? it->second : 0;
}

int ConnectionLimiter::GetBanCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return banned_.size();
}