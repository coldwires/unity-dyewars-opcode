#include <iostream>
#include "server/GameServer.h"
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
    try {
        asio::io_context io_context;
        GameServer server(io_context);
        io_context.run();
    } catch (const std::exception& e) {
        Log::Error("Exception: {}", e.what());
    }
    return 0;
}