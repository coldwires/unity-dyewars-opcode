#include "ClientManager.h"
#include "ClientConnection.h"
#include "core/Log.h"


void ClientManager::AddClient(const std::shared_ptr<ClientConnection> &client) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		clients_[client->GetPlayerID()] = client;
	}
}

void ClientManager::RemoveClient(uint32_t client_id) {
	std::lock_guard<std::mutex> lock(mutex_);
	clients_.erase(client_id);
}

std::shared_ptr<ClientConnection> ClientManager::GetClientCopy(uint32_t client_id) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = clients_.find(client_id);
	return it != clients_.end() ? it->second : nullptr;
}

void ClientManager::BroadcastToOthers(
	uint32_t exclude_id,
	const std::function<void(const std::shared_ptr<ClientConnection>&)>& action) {

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
	for(const auto &client : clients)
	{
		action(client);
	}
}

void ClientManager::BroadcastToAll(
		const std::function<void(const std::shared_ptr<ClientConnection>&)> &action) {

	std::vector<std::shared_ptr<ClientConnection>> clients;
	{
	std::lock_guard<std::mutex> lock(mutex_);
	clients.reserve(clients_.size());
	for (const auto &[id,conn] : clients_)
		clients.push_back(conn);
	} //Lock released

	for(const auto &conn : clients)
		action(conn);
}

size_t ClientManager::Count()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return clients_.size();
}

/// <summary>
/// Closes all client connections
/// </summary>
void ClientManager::CloseAll() {
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto& [id, conn] : clients_)
		conn->Disconnect("Mass D/C From ClientManager");
}

