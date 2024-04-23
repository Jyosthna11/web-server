#include "server.h"

int main() {
    Server server;
    server.setPort(8080);
    server.setDocumentRoot("www");

    if (!server.start()) {
        std::cerr << "Failed to start the server." << std::endl;
        return 1;
    }

    std::cout << "Server started successfully. Listening on port 8080..." << std::endl;

    server.waitForShutdown();

    return 0;
}
