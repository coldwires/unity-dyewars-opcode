#include <iostream>
#include <memory>
#include <thread>
#include "server/GameServer.h"
#include "network/BandwidthMonitor.h"
#include "core/Log.h"
#ifdef _WIN32
#include <windows.h>
#endif

int main() {
    /// Only for displaying color in terminal
#ifdef _WIN32
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),
        ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::unique_ptr<asio::io_context> io_context;
    std::unique_ptr<GameServer> server;
    std::thread io_thread;

    auto start_server = [&]() {
        if (server) {
            Log::Warn("Server already running");
            return;
        }
        try {
            io_context = std::make_unique<asio::io_context>();
            server = std::make_unique<GameServer>(*io_context);
            io_thread = std::thread([&]() { io_context->run(); });
            Log::Info("Server started.");
        }
        catch (const std::exception& e) {
            Log::Error("Failed to start server: {}", e.what());
            server.reset();
            io_context.reset();
        }
        };
    auto stop_server = [&]() {
        if (!server)
        {
            Log::Warn("Server not running");
            return;
        }
        server->Shutdown();
        if (io_thread.joinable()) io_thread.join();
        server.reset();
        io_context.reset();
        Log::Info("Server stopped.");
        };

    //auto-start on launch
    start_server();

    //Console Loop
    std::string cmd;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, cmd)) break;

        if (cmd == "start") {
            start_server();
        }
        else if (cmd == "stop" || cmd == "q") {
            stop_server();
        }
        else if (cmd == "restart") {
            stop_server();
            start_server();
        }
        else if (cmd == "exit" || cmd == "quit") {
            stop_server();
            break;
        }
        else if (cmd == "r") {
            if (server) server->ReloadScripts();
            else Log::Warn("Server not running.");
        }
        else if (cmd == "stats") {
            std::cout << BandwidthMonitor::Instance().GetStats() << std::endl;
        }
        else if (cmd == "status") {
            Log::Info("Server is {}", server ? "running" : "stopped");
        }
        else if (cmd == "help") {
            std::cout << "Commands: start, stop, restart, r (reload), stats, status, exit\n";
        }
    }

    return 0;

}
