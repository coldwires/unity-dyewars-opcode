using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;
using DyeWars.Network;
using DyeWars.Network.Outbound;

/// <summary>
/// Tests ServiceLocator functionality including mock swapping for testing.
/// Demonstrates how to use ServiceLocator to inject mock dependencies.
/// </summary>
public class ServiceLocatorTests
{
    [SetUp]
    public void SetUp()
    {
        ServiceLocator.Clear();
    }

    [TearDown]
    public void TearDown()
    {
        ServiceLocator.Clear();
    }

    // ====================================================================
    // Basic ServiceLocator Tests
    // ====================================================================

    [Test]
    public void Register_And_Get_ReturnsService()
    {
        var mock = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock);

        var retrieved = ServiceLocator.Get<INetworkService>();

        Assert.AreSame(mock, retrieved);
    }

    [Test]
    public void Has_ReturnsTrueWhenRegistered()
    {
        Assert.IsFalse(ServiceLocator.Has<INetworkService>());

        ServiceLocator.Register<INetworkService>(new MockNetworkService());

        Assert.IsTrue(ServiceLocator.Has<INetworkService>());
    }

    [Test]
    public void Unregister_RemovesService()
    {
        ServiceLocator.Register<INetworkService>(new MockNetworkService());
        Assert.IsTrue(ServiceLocator.Has<INetworkService>());

        ServiceLocator.Unregister<INetworkService>();

        Assert.IsFalse(ServiceLocator.Has<INetworkService>());
    }

    [Test]
    public void Clear_RemovesAllServices()
    {
        ServiceLocator.Register<INetworkService>(new MockNetworkService());
        ServiceLocator.Register<ITestService>(new TestServiceImpl());

        ServiceLocator.Clear();

        Assert.IsFalse(ServiceLocator.Has<INetworkService>());
        Assert.IsFalse(ServiceLocator.Has<ITestService>());
    }

    [Test]
    public void Get_ReturnsNull_WhenNotRegistered()
    {
        // Expect the error log from ServiceLocator
        LogAssert.Expect(LogType.Error, "ServiceLocator: Service of type INetworkService not found!");

        var result = ServiceLocator.Get<INetworkService>();
        Assert.IsNull(result);
    }

    // ====================================================================
    // Mock Swapping Tests - The Key Feature
    // ====================================================================

    [Test]
    public void MockSwap_CanReplaceRealServiceWithMock()
    {
        // In real code, NetworkService registers itself
        // In tests, we register a mock instead
        var mock = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock);

        // Code that uses ServiceLocator.Get<INetworkService>() now gets the mock
        var service = ServiceLocator.Get<INetworkService>();

        Assert.IsInstanceOf<MockNetworkService>(service);
        Assert.IsFalse(service.IsConnected); // Mock starts disconnected
    }

    [Test]
    public void MockSwap_CanControlMockBehavior()
    {
        var mock = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock);

        // Get service and verify initial state
        var service = ServiceLocator.Get<INetworkService>();
        Assert.IsFalse(service.IsConnected);

        // Simulate connection
        mock.SimulateConnect();

        // Verify state changed
        Assert.IsTrue(service.IsConnected);
    }

    [Test]
    public void MockSwap_CanTrackMethodCalls()
    {
        var mock = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock);

        var service = ServiceLocator.Get<INetworkService>();

        // Call methods
        service.Connect("localhost", 8080);
        service.Connect("192.168.1.1", 9000);
        service.Disconnect();

        // Verify calls were tracked
        Assert.AreEqual(2, mock.ConnectCallCount);
        Assert.AreEqual(1, mock.DisconnectCallCount);
        Assert.AreEqual("192.168.1.1", mock.LastConnectHost);
        Assert.AreEqual(9000, mock.LastConnectPort);
    }

    [Test]
    public void MockSwap_CanOverwriteExistingRegistration()
    {
        // First registration
        var mock1 = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock1);

        // Overwrite with second registration
        var mock2 = new MockNetworkService();
        mock2.SimulateConnect();
        ServiceLocator.Register<INetworkService>(mock2);

        // Should get the second mock
        var service = ServiceLocator.Get<INetworkService>();
        Assert.AreSame(mock2, service);
        Assert.IsTrue(service.IsConnected);
    }

    // ====================================================================
    // Integration Test Examples
    // ====================================================================

    [Test]
    public void Integration_SystemCanUseInjectedMock()
    {
        // Setup mock
        var mock = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock);

        // Simulate a system that uses the network service
        var system = new ExampleSystemUnderTest();
        system.Initialize();

        // System should have gotten the mock
        system.DoSomethingThatNeedsNetwork();

        // Verify the mock received the call
        Assert.AreEqual(1, mock.ConnectCallCount);
    }

    [Test]
    public void Integration_CanTestDisconnectedScenario()
    {
        var mock = new MockNetworkService();
        ServiceLocator.Register<INetworkService>(mock);

        var system = new ExampleSystemUnderTest();
        system.Initialize();

        // Mock is disconnected by default
        bool result = system.TrySendMessage("Hello");

        Assert.IsFalse(result); // Should fail because not connected
    }

    [Test]
    public void Integration_CanTestConnectedScenario()
    {
        var mock = new MockNetworkService();
        mock.SimulateConnect();
        ServiceLocator.Register<INetworkService>(mock);

        var system = new ExampleSystemUnderTest();
        system.Initialize();

        bool result = system.TrySendMessage("Hello");

        Assert.IsTrue(result); // Should succeed because connected
    }
}

// ============================================================================
// Mock Implementations
// ============================================================================

/// <summary>
/// Mock implementation of INetworkService for testing.
/// Tracks method calls and allows simulating connection state.
/// </summary>
public class MockNetworkService : INetworkService
{
    // Simulated state
    private bool isConnected = false;

    // Call tracking
    public int ConnectCallCount { get; private set; }
    public int DisconnectCallCount { get; private set; }
    public string LastConnectHost { get; private set; }
    public int LastConnectPort { get; private set; }

    // INetworkService implementation
    public PacketSender Sender => null; // Return null or create a MockPacketSender if needed
    public bool IsConnected => isConnected;

    public void Connect(string host, int port)
    {
        ConnectCallCount++;
        LastConnectHost = host;
        LastConnectPort = port;
        // Don't actually connect - this is a mock
    }

    public void Disconnect()
    {
        DisconnectCallCount++;
        isConnected = false;
    }

    // Test helpers
    public void SimulateConnect()
    {
        isConnected = true;
    }

    public void SimulateDisconnect()
    {
        isConnected = false;
    }

    public void Reset()
    {
        isConnected = false;
        ConnectCallCount = 0;
        DisconnectCallCount = 0;
        LastConnectHost = null;
        LastConnectPort = 0;
    }
}

/// <summary>
/// Example interface for testing multiple service types.
/// </summary>
public interface ITestService
{
    void DoWork();
}

public class TestServiceImpl : ITestService
{
    public void DoWork() { }
}

/// <summary>
/// Example system that uses ServiceLocator to get dependencies.
/// Demonstrates how real game systems would be tested.
/// </summary>
public class ExampleSystemUnderTest
{
    private INetworkService networkService;

    public void Initialize()
    {
        networkService = ServiceLocator.Get<INetworkService>();
    }

    public void DoSomethingThatNeedsNetwork()
    {
        networkService?.Connect("localhost", 8080);
    }

    public bool TrySendMessage(string message)
    {
        if (networkService == null || !networkService.IsConnected)
            return false;

        // Would send message here
        return true;
    }
}
