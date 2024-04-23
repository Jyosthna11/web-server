#include "server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <iomanip>
#include <ctime>

Server::Server() : serverSocket(-1), port(8080), documentRoot("www"), running(false) {}

Server::~Server() {
    stop();
}

void writeToLogFile(const std::string& data) {
    // Open a log file in append mode
    std::ofstream logFile("www/contactinfo.txt", std::ios_base::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file." << std::endl;
        return;
    }

    // Get current date and time
    std::time_t currentTime = std::time(nullptr);
    std::tm* localTime = std::localtime(&currentTime);

    // Format the date and time
    std::ostringstream formattedTime;
    formattedTime << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");

    std::istringstream iss(data);
    std::vector<std::string> keyValuePairs;
    std::string pair;
    while (std::getline(iss, pair, '&')) {
        keyValuePairs.push_back(pair);
    }

    // Write the data to the log file
    logFile << "Date and Time: " << formattedTime.str() << std::endl;
    logFile << "\n";
    for (const std::string& kvPair : keyValuePairs) {
        std::istringstream pairStream(kvPair);
        std::string key, value;
        std::getline(pairStream, key, '=');
        std::getline(pairStream, value);
        logFile << key << ": " << value << std::endl;
    }
    logFile << "----------------------------------------" << std::endl;

    // Close the log file
    logFile.close();
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
        return true; // Server is already running, no need to start again
    }

    int initialPort = port;
    int maxPortAttempts = 10; // Maximum number of attempts to find a free port
    int portOffset = 0;

    while (portOffset < maxPortAttempts) {
        try {
            if (bindSocket()) {
                running = true;

                // Start accepting connections in a separate thread
                std::thread acceptThread(&Server::acceptConnections, this);
                acceptThread.detach();

                std::cout << "Server started successfully on port " << port << "." << std::endl;
                return true; // Server started successfully
            }
        } catch (const std::exception& e) {
            // Handle the exception locally or re-throw it
            std::cerr << "Error starting server: " << e.what() << std::endl;
            return false; // Unable to start server due to an error
        }

        // Retry with a different port
        port = initialPort + portOffset;
        portOffset++;
    }

    std::cerr << "Failed to start server. Unable to bind the socket to any available port." << std::endl;
    return false; // Unable to start server after maximum port attempts
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
        throw std::runtime_error("Failed to create socket.");
    }

    // Set socket options to allow reuse of address and port
    int optval = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        close(serverSocket);
        throw std::runtime_error("Failed to set socket options.");
    }

    // Bind address
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
        close(serverSocket);
        throw std::runtime_error("Failed to bind the socket. Port may already be in use.");
    }

    // Start listening
    if (listen(serverSocket, SOMAXCONN) == -1) {
        close(serverSocket);
        throw std::runtime_error("Failed to listen on socket.");
    }

    return true;
}

void Server::acceptConnections() {
    while (running) {
        try {
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
        } catch (const std::exception& e) {
            // Handle the exception locally or re-throw it
            std::cerr << "Error accepting connection: " << e.what() << std::endl;
        }
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

    // Extract requested file path from the request
    std::string request(buffer, bytesRead);
    std::istringstream requestStream(request);
    std::string method, path, protocol;
    requestStream >> method >> path >> protocol;

    // Remove leading slash from the path
    path = path.substr(1);

    if (path == "") {
        std::string welcomeMessage = "<html><head><title>Welcome to the Server</title></head>";
        welcomeMessage += "<body><h1>Welcome to the Server</h1>";
        welcomeMessage += "<p>The server is running successfully.</p>";
        welcomeMessage += "<p>Click <a href=\"/contact.html\">here</a> to navigate to another page.</p>";
        welcomeMessage += "</body></html>";

        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(welcomeMessage.size()) + "\r\n\r\n" + welcomeMessage;

        ssize_t bytesSent = send(clientSocket, response.c_str(), response.size(), 0);
        if (bytesSent != response.size()) {
            std::cerr << "Failed to send response to client." << std::endl;
        }

        close(clientSocket);
        return;
    }

    // Check if the request method is POST and the path is '/submit'
    if (method == "POST" && path == "submit") {
        size_t pos = request.find("\r\n\r\n");
        if (pos == std::string::npos) {
            // Invalid request
            std::cerr << "Invalid POST request." << std::endl;
            close(clientSocket);
            return;
        }

        // Extract POST data from the request body
        std::string postData = request.substr(pos + 4);
        writeToLogFile(postData);

        // Send a response back to the client
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        response += "<html><head><title>Contact Form Submitted</title></head>";
        response += "<body><p>Below are the details submitted</p>";
        response += postData;
        response += "<h1>Thank you for your submission!</h1></body></html>";

        ssize_t bytesSent = send(clientSocket, response.c_str(), response.size(), 0);
        if (bytesSent != response.size()) {
            std::cerr << "Failed to send response to client." << std::endl;
        }
    } else {
        // Build the full path to the requested file
        std::string fullPath = documentRoot + "/" + path;

        // Check if the requested file exists
        std::string filePath = documentRoot + "/" + path;
        std::ifstream file(filePath);
        if (!file) {
            // File not found, send 404 response
            std::string notFoundResponse = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
            notFoundResponse += "<html><head><title>404 Not Found</title></head>";
            notFoundResponse += "<body><h1>404 Not Found</h1><p>The requested resource could not be found.</p></body></html>";

            ssize_t bytesSent = send(clientSocket, notFoundResponse.c_str(), notFoundResponse.size(), 0);
            if (bytesSent != notFoundResponse.size()) {
                std::cerr << "Failed to send 404 response to client." << std::endl;
            }

            close(clientSocket);
            return;
        }

        // Read the file content
        std::ostringstream fileContentStream;
        fileContentStream << file.rdbuf();
        std::string fileContent = fileContentStream.str();

        // Build the HTTP response
        std::ostringstream responseStream;
        responseStream << "HTTP/1.1 200 OK\r\n";
        responseStream << "Content-Length: " << fileContent.size() << "\r\n";
        responseStream << "Content-Type: text/html\r\n\r\n";
        responseStream << fileContent;

        std::string response = responseStream.str();

        // Send response to client
        ssize_t bytesSent = send(clientSocket, response.c_str(), response.size(), 0);
        if (bytesSent != response.size()) {
            std::cerr << "Failed to send response to client." << std::endl;
        }
    }

    // Close client socket
    close(clientSocket);
}

