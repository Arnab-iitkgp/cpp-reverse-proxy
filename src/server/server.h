#pragma once 
#include <string>
#include <winsock2.h>
#include "../concurrency/thread_pool.h"

class TCPServer {
    private:
    std:: string ip_address;
    int port;
    SOCKET serverSocket;
    ThreadPool pool;
    public:
    TCPServer(std::string ip_address, int port);
    ~TCPServer();
    bool start();
    void listenForCLient();
};