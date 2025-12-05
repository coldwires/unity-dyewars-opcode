#pragma once
#include <memory>
#include <map>
#include <mutex>
#include <functional>

class ClientConnection;

class ClientManager {
public:

	/// <summary>
	/// adds client to client list
	/// </summary>
	void AddClient(const std::shared_ptr<ClientConnection> &client);
	
	/// <summary>
	/// Removes client
	/// </summary>
	void RemoveClient(uint64_t client_id);

	/// <summary>
	/// Close all client sockets
	/// </summary>
	void CloseAll();

	std::shared_ptr<ClientConnection> GetClientCopy(uint32_t client_id);
	ClientConnection* GetClientPtr(uint32_t id);

	void BroadcastToOthers(uint32_t exclude_id, const std::function<void(const std::shared_ptr<ClientConnection>&)>& action);
	void BroadcastToAll(const std::function<void(const std::shared_ptr<ClientConnection>&)> &action);

	size_t Count();

private:

	std::map<uint32_t, std::shared_ptr<ClientConnection>> clients_;
	std::mutex mutex_;
};