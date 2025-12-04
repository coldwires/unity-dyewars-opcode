#include "ClientManager.h"
#include "ClientConnection.h"
#include "core/Log.h"

void ClientManager::Register() {
	
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
	const std::function<void(std::shared_ptr<ClientConnection>)>& action) {

	std::lock_guard<std::mutex> lock(mutex_);
	for (const auto& [id, conn] : clients_) {
		if (id != exclude_id)
			action(conn);
	}
}

void ClientManager::BroadcastToAll(std::function<void(std::shared_ptr<ClientConnection>)> action) {
	std::lock_guard<std::mutex> lock(mutex_);
	for (const auto& pair : clients_) action(pair.second);
}