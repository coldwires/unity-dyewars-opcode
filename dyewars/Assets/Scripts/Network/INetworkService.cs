// INetworkService.cs
// Interface defining what a network service must provide.
// Using an interface allows us to swap implementations (e.g., for testing with a mock).
//
// The real implementation (NetworkService) handles TCP connections.
// A test implementation could simulate network behavior without actual connections.

using DyeWars.Network.Outbound;

namespace DyeWars.Network
{
    public interface INetworkService
    {

        PacketSender Sender { get; }

        /// <summary>
        /// Whether we're currently connected to the server.
        /// </summary>
        bool IsConnected { get; }


        /// <summary>
        /// Connect to the game server.
        /// </summary>
        void Connect(string host, int port);

        /// <summary>
        /// Disconnect from the server.
        /// </summary>
        void Disconnect();

    }
}