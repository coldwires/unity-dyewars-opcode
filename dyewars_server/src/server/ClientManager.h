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
	void Register();
	
	/// <summary>
	/// Removes client from map
	/// </summary>
	void RemoveClient(uint32_t player_id);

	/// <summary>
	/// Close all client sockets
	/// </summary>
	void CloseAll();

	uint32_t GenerateUniquePlayerID();

	void BroadcastToOthers(uint32_t exclude_id, const std::function<void(std::shared_ptr<ClientConnection>)>& action);

	void BroadcastToAll(std::function<void(std::shared_ptr<ClientConnection>)> action);

	ClientConnection* GetClient(uint32_t id);
	std::vector<PlayerData> GetAllPlayerData();

	

	size_t Count();

private:

	std::map<uint32_t, std::shared_ptr<ClientConnection>> clients_;
	std::mutex mutex_;

	std::mt19937 rng_{ std::random_device{}() };
	std::uniform_int_distribution<uint32_t> id_dist_{ 1, 0xFFFFFFFF };
};