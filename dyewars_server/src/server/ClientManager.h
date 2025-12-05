#pragma once
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <random>
#include <functional>
#include "core/Common.h"

class ClientConnection;

class ClientManager {
public:

	/// <summary>
	/// adds client to client list
	/// </summary>
	void AddClient(std::shared_ptr<ClientConnection> client);
	
	/// <summary>
	/// Removes client
	/// </summary>
	void RemoveClient(uint32_t player_id);

	/// <summary>
	/// Close all client sockets
	/// </summary>
	void CloseAll();

	std::shared_ptr<ClientConnection> GetClientCopy(uint32_t client_id);
	ClientConnection* GetClientPtr(uint32_t id);

	// TODO Move to player registry
	uint32_t GenerateUniquePlayerID();

	void BroadcastToOthers(uint32_t exclude_id, const std::function<void(const std::shared_ptr<ClientConnection>&)>& action);
	void BroadcastToAll(const std::function<void(const std::shared_ptr<ClientConnection>&)> &action);

	size_t Count();

private:

	std::map<uint32_t, std::shared_ptr<ClientConnection>> clients_;
	std::mutex mutex_;

	//TODO Move to player registry
	std::mt19937 rng_{ std::random_device{}() };
	std::uniform_int_distribution<uint32_t> id_dist_{ 1, 0xFFFFFFFF };
};