// INetworkService.cs
// Interface defining what a network service must provide.
// Using an interface allows us to swap implementations (e.g., for testing with a mock).
//
// The real implementation (NetworkService) handles TCP connections.
// A test implementation could simulate network behavior without actual connections.

namespace DyeWars.Network
{
    public interface INetworkService
    {
        /// <summary>
        /// Whether we're currently connected to the server.
        /// </summary>
        bool IsConnected { get; }

        /// <summary>
        /// Our assigned player ID (0 if not yet assigned).
        /// </summary>
        uint LocalPlayerId { get; }

        /// <summary>
        /// Connect to the game server.
        /// </summary>
        void Connect(string host, int port);

        /// <summary>
        /// Disconnect from the server.
        /// </summary>
        void Disconnect();

        /// <summary>
        /// Send a move command to the server.
        /// </summary>
        void SendMove(int direction, int facing);

        /// <summary>
        /// Send a turn command to the server.
        /// </summary>
        void SendTurn(int direction);
    }
}