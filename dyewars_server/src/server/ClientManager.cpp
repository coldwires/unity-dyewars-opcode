#include "ClientManager.h"
#include "ClientConnection.h"
#include "core/Log.h"

void ClientManager::AddClient(const std::shared_ptr<ClientConnection> &client) {
    assert(client && "AddClient: null client");
    assert(client->GetClientID() != 0 && "AddClient: invalid ID");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[client->GetClientID()] = client;
    }// Move Log outside locks
    Log::Debug("Client {} added to manager", client->GetClientID());
}

void ClientManager::RemoveClient(uint64_t client_id) {
    assert(client_id != 0);
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (clients_.erase(client_id) > 0) {
            removed = true;
        }
    }
    if (removed) Log::Debug("Client {} removed from manager", client_id);
}

std::shared_ptr<ClientConnection> ClientManager::GetClient(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_id);
    return it != clients_.end() ? it->second : nullptr;
}

void ClientManager::BroadcastToOthers(
        uint64_t exclude_id,
        const std::function<void(const std::shared_ptr<ClientConnection> &)> &action) {

    std::vector<std::shared_ptr<ClientConnection>> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        /// Without reserve, the vector doubles in size each time it runs out of space
        /// — allocates new memory, copies everything, frees old memory.
        /// With 1000 clients, that's several reallocations.
        clients.reserve(clients_.size());

        for (const auto &[id, conn]: clients_) {
            if (id != exclude_id)
                clients.push_back(conn);
        }
    } // Release lock
    for (const auto &client: clients) {
        action(client);
    }
}

void ClientManager::BroadcastToAll(
        const std::function<void(const std::shared_ptr<ClientConnection> &)> &action) {

    std::vector<std::shared_ptr<ClientConnection>> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients.reserve(clients_.size());
        for (const auto &[id, conn]: clients_)
            clients.push_back(conn);
    } //Lock released

    for (const auto &conn: clients)
        action(conn);
}

size_t ClientManager::Count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size();
}

/// <summary>
/// Closes all client connections during server shutdown.
/// </summary>
void ClientManager::CloseAll() {
    std::unordered_map<uint64_t, std::shared_ptr<ClientConnection>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Use swap instead of move.
        // std::move leaves clients_ in an "unspecified but valid" state,
        // which could be non-empty on some implementations.
        // swap guarantees clients_ is empty and snapshot has the contents.
        // This is safer if any code accesses clients_ after CloseAll.
        snapshot.swap(clients_);
    }// Lock released

    for (auto &[id, conn]: snapshot) {
        conn->CloseSocket();
    }
    // Why CloseSocket and not Disconnect?
    // Disconnect tries to lock ClientManager and PlayerRegistry to clean up state.
    // But we're shutting down - those structures are being torn down too.
    // CloseSocket just closes the TCP connection, which is all we need.
    // The shared_ptrs in snapshot will be released when this function returns,
    // cleaning up the ClientConnection objects naturally.
} // snapshot dies here, dropping all shared_ptrs