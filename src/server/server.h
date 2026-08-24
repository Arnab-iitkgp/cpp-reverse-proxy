#pragma once 
#include <string>
#include <winsock2.h>

class TCPServer {
    private:
    std:: string ip_address;
    int port;
    SOCKET serverSocket;

    public:
    TCPServer(std::string ip_address, int port);
    ~TCPServer();
    bool start();
    void listenForCLient();
};