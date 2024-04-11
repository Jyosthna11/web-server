#include "server.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>

Server::Server() : serverSocket(-1), port(8080), documentRoot("www"), running(false) {}

Server::~Server() {
    stop();
}

void Server::setPort(int port) {
    this->port = port;
}

void Server::setDocumentRoot(const std::string& documentRoot) {
    this->documentRoot = documentRoot;
}

bool Server::start() {
    if (running) {
        std::cerr << "Server is already running." << std::endl;
        return false;
    }

    if (!bindSocket()) {
        return false;
    }

    running = true;

    // Start accepting connections in a separate thread
    std::thread acceptThread(&Server::acceptConnections, this);
    acceptThread.detach();

    return true;
}

void Server::stop() {
    if (!running) {
        std::cerr << "Server is not running." << std::endl;
        return;
    }

    close(serverSocket);
    running = false;
}

void Server::waitForShutdown() {
    while (running) {
        // Wait for termination signal
        usleep(100000); // Sleep for 100ms
    }
}

bool Server::bindSocket() {
    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return false;
    }

    // Set socket options to allow reuse of address and port
    int optval = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Bind address
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
        std::cerr << "Failed to bind socket." << std::endl;
        close(serverSocket);
        return false;
    }

    // Start listening
    if (listen(serverSocket, SOMAXCONN) == -1) {
        std::cerr << "Failed to listen on socket." << std::endl;
        close(serverSocket);
        return false;
    }

    return true;
}

void Server::acceptConnections() {
    while (running) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == -1) {
            std::cerr << "Failed to accept connection." << std::endl;
            continue;
        }

        // Create a new thread to handle the client connection
        std::thread clientThread(&Server::handleClient, this, clientSocket);
        clientThread.detach();
    }
}

void Server::handleClient(int clientSocket) {
    // Read request from client
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0) {
        std::cerr << "Failed to read from client." << std::endl;
        close(clientSocket);
        return;
    }

    // Process request
    // For simplicity, let's assume the request is always GET and just serve a simple HTML page
    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    response += "<html><head><title>Simple Web Server</title></head>";
    response += "<body><h1>Welcome to the Simple Web Server</h1></body></html>";

    // Send response to client
    ssize_t bytesSent = send(clientSocket, response.c_str(), response.size(), 0);
    if (bytesSent != response.size()) {
        std::cerr << "Failed to send response to client." << std::endl;
    }

    // Close client socket
    close(clientSocket);
}
