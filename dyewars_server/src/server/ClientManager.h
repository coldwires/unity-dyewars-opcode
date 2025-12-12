#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <variant>
#include <optional>

class ClientConnection;
class FakeClientConnection;

/// Connection variant - can hold either real or fake connection
using AnyConnection = std::variant<
    std::shared_ptr<ClientConnection>,
    std::shared_ptr<FakeClientConnection>
>;

/// Manages all active client connections (real and fake).
/// Thread-safe: all methods acquire mutex internally.
class ClientManager {
public:
    void AddClient(const std::shared_ptr<ClientConnection> &client);
    void AddFakeClient(const std::shared_ptr<FakeClientConnection> &client);
    void RemoveClient(uint64_t client_id);
    void CloseAll();

    /// Get real client by ID. Returns nullptr if not found or if it's a fake client.
    /// Returns shared_ptr for safe cross-thread access.
    std::shared_ptr<ClientConnection> GetClient(uint64_t client_id);

    /// Get any connection (real or fake) by ID.
    /// Returns std::nullopt if not found.
    std::optional<AnyConnection> GetAnyClient(uint64_t client_id);

    /// Batch lookup: Get all connections for a set of IDs in one lock acquisition.
    /// Returns both real and fake connections.
    template<typename MapType>
    std::unordered_map<uint64_t, AnyConnection> GetAnyClientsForIDs(const MapType& id_map) {
        std::unordered_map<uint64_t, AnyConnection> result;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [client_id, _] : id_map) {
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                result[client_id] = it->second;
            }
        }
        return result;
    }

    /// Legacy batch lookup - only returns real connections
    template<typename MapType>
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnection>> GetClientsForIDs(const MapType& id_map) {
        std::unordered_map<uint64_t, std::shared_ptr<ClientConnection>> result;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [client_id, _] : id_map) {
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                if (auto* real = std::get_if<std::shared_ptr<ClientConnection>>(&it->second)) {
                    result[client_id] = *real;
                }
            }
        }
        return result;
    }

    void BroadcastToOthers(uint64_t exclude_id,
                           const std::function<void(const std::shared_ptr<ClientConnection> &)> &action);

    void BroadcastToAll(const std::function<void(const std::shared_ptr<ClientConnection> &)> &action);

    /// Broadcast to all connections including fake ones
    void BroadcastToAllIncludingFake(
        const std::function<void(const std::shared_ptr<ClientConnection>&)>& real_action,
        const std::function<void(const std::shared_ptr<FakeClientConnection>&)>& fake_action);

    size_t Count();
    size_t RealCount();
    size_t FakeCount();

private:
    std::unordered_map<uint64_t, AnyConnection> clients_;
    std::mutex mutex_;
    size_t fake_count_ = 0;
};