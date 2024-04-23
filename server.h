#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <netinet/in.h>
#include <unistd.h> 



class Server {
public:
    Server();
    ~Server();

    void setPort(int port);
    void setDocumentRoot(const std::string& documentRoot);

    bool start();
    void stop();
    void waitForShutdown();

private:
    int serverSocket;
    int port;
    std::string documentRoot;
    bool running;
    std::vector<std::thread> workerThreads;

    bool bindSocket();
    void acceptConnections();
    void handleClient(int clientSocket);
};

#endif // SERVER_H
