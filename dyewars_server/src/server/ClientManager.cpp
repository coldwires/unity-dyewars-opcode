#include "ClientManager.h"
#include "ClientConnection.h"
#include "FakeClientConnection.h"
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

void ClientManager::AddFakeClient(const std::shared_ptr<FakeClientConnection> &client) {
    assert(client && "AddFakeClient: null client");
    assert(client->GetClientID() != 0 && "AddFakeClient: invalid ID");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[client->GetClientID()] = client;
        fake_count_++;
    }
    Log::Trace("Fake client {} added to manager", client->GetClientID());
}

void ClientManager::RemoveClient(uint64_t client_id) {
    assert(client_id != 0);
    bool removed = false;
    bool was_fake = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            was_fake = std::holds_alternative<std::shared_ptr<FakeClientConnection>>(it->second);
            clients_.erase(it);
            removed = true;
            if (was_fake && fake_count_ > 0) fake_count_--;
        }
    }
    if (removed) Log::Debug("Client {} removed from manager", client_id);
}

std::shared_ptr<ClientConnection> ClientManager::GetClient(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) return nullptr;
    if (auto* real = std::get_if<std::shared_ptr<ClientConnection>>(&it->second)) {
        return *real;
    }
    return nullptr;  // It's a fake client
}

std::optional<AnyConnection> ClientManager::GetAnyClient(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(client_id);
    if (it == clients_.end()) return std::nullopt;
    return it->second;
}

void ClientManager::BroadcastToOthers(
        uint64_t exclude_id,
        const std::function<void(const std::shared_ptr<ClientConnection> &)> &action) {

    std::vector<std::shared_ptr<ClientConnection>> clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients.reserve(clients_.size());

        for (const auto &[id, conn]: clients_) {
            if (id != exclude_id) {
                if (auto* real = std::get_if<std::shared_ptr<ClientConnection>>(&conn)) {
                    clients.push_back(*real);
                }
            }
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
        for (const auto &[id, conn]: clients_) {
            if (auto* real = std::get_if<std::shared_ptr<ClientConnection>>(&conn)) {
                clients.push_back(*real);
            }
        }
    } //Lock released

    for (const auto &conn: clients)
        action(conn);
}

void ClientManager::BroadcastToAllIncludingFake(
        const std::function<void(const std::shared_ptr<ClientConnection>&)>& real_action,
        const std::function<void(const std::shared_ptr<FakeClientConnection>&)>& fake_action) {

    std::vector<std::shared_ptr<ClientConnection>> real_clients;
    std::vector<std::shared_ptr<FakeClientConnection>> fake_clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        real_clients.reserve(clients_.size() - fake_count_);
        fake_clients.reserve(fake_count_);

        for (const auto &[id, conn]: clients_) {
            if (auto* real = std::get_if<std::shared_ptr<ClientConnection>>(&conn)) {
                real_clients.push_back(*real);
            } else if (auto* fake = std::get_if<std::shared_ptr<FakeClientConnection>>(&conn)) {
                fake_clients.push_back(*fake);
            }
        }
    }

    for (const auto &conn: real_clients)
        real_action(conn);
    for (const auto &conn: fake_clients)
        fake_action(conn);
}

size_t ClientManager::Count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size();
}

size_t ClientManager::RealCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size() - fake_count_;
}

size_t ClientManager::FakeCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return fake_count_;
}

void ClientManager::CloseAll() {
    std::unordered_map<uint64_t, AnyConnection> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.swap(clients_);
        fake_count_ = 0;
    }// Lock released

    for (auto &[id, conn]: snapshot) {
        // Only close real connections
        if (auto* real = std::get_if<std::shared_ptr<ClientConnection>>(&conn)) {
            (*real)->CloseSocket();
        }
        // Fake connections just get dropped
    }
} // snapshot dies here, dropping all shared_ptrs