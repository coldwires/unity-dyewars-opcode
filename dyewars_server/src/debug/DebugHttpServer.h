/// =======================================
/// DyeWarsServer - Debug HTTP Server
///
/// Simple HTTP server for viewing server stats in a browser.
/// Runs on a separate port (default 8081) and serves:
/// - GET /stats - JSON stats
/// - GET / - HTML dashboard
///
/// Created by Anonymous on Dec 11, 2025
/// =======================================
#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <atomic>

/// Stats provider function type
/// Returns JSON string with current server stats
using StatsProvider = std::function<std::string()>;

/// Simple HTTP server for debug dashboard
class DebugHttpServer {
public:
    /// Create debug server on specified port
    /// @param io_context ASIO context (can share with main server)
    /// @param port HTTP port (default 8081)
    explicit DebugHttpServer(asio::io_context& io_context, uint16_t port = 8081);

    ~DebugHttpServer();

    /// Set the function that provides stats JSON
    void SetStatsProvider(StatsProvider provider);

    /// Start accepting connections
    void Start();

    /// Stop the server
    void Stop();

private:
    void StartAccept();
    void HandleConnection(asio::ip::tcp::socket socket);
    std::string BuildResponse(const std::string& path);
    std::string GetDashboardHtml();
    std::string GetStatsJson();

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    StatsProvider stats_provider_;
};