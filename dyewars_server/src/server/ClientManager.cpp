#include "ClientManager.h"
#include "ClientConnection.h"
#include "core/Log.h"


void ClientManager::AddClient(const std::shared_ptr<ClientConnection> &client) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		clients_[client->GetPlayerID()] = client;
	}
}

uint32_t ClientManager::GenerateUniquePlayerID()
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (int attempts = 0; attempts < 150; attempts++)
	{
		uint32_t id = id_dist_(rng_);
		if (!clients_.contains(id)) {
			return id;
		}
	}
	Log::Error("Failed to generate unique ID");
	return 0;
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