#include <iostream>
#include "include/server/GameServer.h"

int main() {
    try {
        asio::io_context io_context;
        GameServer server(io_context, 8080);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}