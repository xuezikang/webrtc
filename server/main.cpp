#include "WebSocketServer.h"
#include <iostream>
#include <csignal>

WebSocketServer* g_server = nullptr;

void signal_handler(int sig) {
    std::cout << "\nShutting down..." << std::endl;
    if (g_server) {
        // Server will stop when main exits
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    signal(SIGINT, signal_handler);

    WebSocketServer server;
    g_server = &server;

    try {
        server.run(port);
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
