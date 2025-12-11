#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>

class ClientConnection;

/// Manages all active client connections.
/// Thread-safe: all methods acquire mutex internally.
class ClientManager {
public:
    void AddClient(const std::shared_ptr<ClientConnection> &client);
    void RemoveClient(uint64_t client_id);
    void CloseAll();

    /// Get client by ID. Returns nullptr if not found.
    /// Returns shared_ptr for safe cross-thread access.
    std::shared_ptr<ClientConnection> GetClient(uint64_t client_id);

    void BroadcastToOthers(uint64_t exclude_id,
                           const std::function<void(const std::shared_ptr<ClientConnection> &)> &action);

    void BroadcastToAll(const std::function<void(const std::shared_ptr<ClientConnection> &)> &action);

    size_t Count();

private:
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnection>> clients_;
    std::mutex mutex_;
};