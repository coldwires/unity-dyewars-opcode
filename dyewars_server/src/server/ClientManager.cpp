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
	if(removed) Log::Debug("Client {} removed from manager", client_id);
}

std::shared_ptr<ClientConnection> ClientManager::GetClientCopy(uint64_t client_id) {
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = clients_.find(client_id);
	return it != clients_.end() ? it->second : nullptr;
}

void ClientManager::BroadcastToOthers(
	uint64_t exclude_id,
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
	std::vector<std::shared_ptr<ClientConnection>> clients;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		clients.reserve(clients_.size());
		for (const auto &[id, conn]: clients_) {
			clients.push_back(conn);
		}
		clients_.clear();
	}// Lock released

	for(auto &conn : clients)
		conn->CloseSocket();
	// Disconnect tries to lock clientmanager, we just deleted it above.
	// Player registry is in weird state, everything is shutting down.
	// Closing socket is the move here. We're closing all connections anyway, no need to
	// Attempt to keep state of lists clean
		//conn->Disconnect("Mass D/C From ClientManager");
}