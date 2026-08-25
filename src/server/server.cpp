#include <thread>
#include "server.h"
#include<iostream>
#include<winsock2.h>
#include<ws2tcpip.h>
#include "../http/request.h"
#include<string>
#include<vector>
#include<mutex>
#include<chrono>
#include<atomic>

TCPServer::TCPServer(std::string ip_address, int port):ip_address(ip_address), port(port),pool(128){
     WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { 
        std::cerr << "WSAStartup failed." << std::endl;
    }
    std::cout<<"Networking initialised succesfully"<<'\n';
    
}
TCPServer::~TCPServer(){
    closesocket(serverSocket);
    WSACleanup();
}
bool TCPServer::start(){
     serverSocket  = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

     if(serverSocket== INVALID_SOCKET){
        std::cerr<<"Error creating socket. "<<std::endl;
        WSACleanup();
        return 1;
    }
    std::cout<<"socket created!\n";

    // assigning the ip and port to the socket -- bind
    sockaddr_in serverAddress;
    serverAddress.sin_family =AF_INET; //ipv4
    // serverAddress.sin_addr.s_addr=ip_address; // listen on any ip my pc has, here we cant write like this, ip is string, but win need in raw binary bytes
    serverAddress.sin_addr.s_addr = inet_addr(ip_address.c_str());
    serverAddress.sin_port = htons(port); // convert to network byte order


        //bind
    if(bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress))==SOCKET_ERROR){
        std::cerr<<"Bind failed! port might already be in use\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // listen
    if(listen(serverSocket, SOMAXCONN)==SOCKET_ERROR){
        std::cerr<<"Listen Failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout<<"Server is listening on PORT"<<port<<"\n";
    return true;
}

// global routing state
std::vector<int>all_backends = {9001,9002,9003}; // list of backend server
std::vector<int> healthy_backends = {9001,9002,9003}; // workers only use this

int current_backend_index = 0;
std::mutex rr_mutex; // protects the current_backend_index

//runs in background and pings server every 5 sec
void backgroundHealthMonitor(){
    while(true){
        std::vector<int>newly_healthy;

        for(int port: all_backends){
            SOCKET testSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            addr.sin_port = htons(port);

            // Ping the server to see if it's alive
            if (connect(testSocket, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
                newly_healthy.push_back(port);
            }
            closesocket(testSocket);
        }
            // safeky update the global list of healthy backends
        {
            std::unique_lock<std::mutex> lock(rr_mutex);
            healthy_backends = newly_healthy;
        }
        // sleep for 5 sec before checing again
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
void handleClient(SOCKET clientSocket){
    // Enable TCP_NODELAY
    int flag = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
    setsockopt(clientSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(int));

    linger sl = {1, 0}; // Instant 0-second teardown — zero TIME_WAIT buildup across runs
    setsockopt(clientSocket, SOL_SOCKET, SO_LINGER, (const char*)&sl, sizeof(sl));

    DWORD clientTimeout = 5000; // 5s idle keep-alive timeout
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&clientTimeout, sizeof(clientTimeout));

    int targetPort = 0;

    // Safely get the next port from the HEALTHY list
    {
        std::unique_lock<std::mutex> lock(rr_mutex);
        if (healthy_backends.empty()) {
            std::cerr << "[Proxy] ALL BACKENDS ARE DOWN! Sending 502 Bad Gateway.\n";
            
            // Send a real HTTP error to the browser
            const char* error502 = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n<h1>502 Bad Gateway</h1><p>All backend servers are currently down.</p>";
            send(clientSocket, error502, strlen(error502), 0);
            
            closesocket(clientSocket);
            return;
        }


        // Wrap around if index went out of bounds (e.g. if a server died)
        if (current_backend_index >= healthy_backends.size()) {
            current_backend_index = 0;
        }

        targetPort = healthy_backends[current_backend_index];
        current_backend_index = (current_backend_index + 1) % healthy_backends.size();
    } // Mutex releases here!

    // We know it's healthy, so we only need to connect ONCE!
    SOCKET backendSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(backendSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
    setsockopt(backendSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(int));
    setsockopt(backendSocket, SOL_SOCKET, SO_LINGER, (const char*)&sl, sizeof(sl));

    // --- 2. Backend Timeout ---
    DWORD backendTimeout = 2000;
    setsockopt(backendSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&backendTimeout, sizeof(backendTimeout));

    sockaddr_in backendAddr;
    backendAddr.sin_family = AF_INET;
    backendAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    backendAddr.sin_port = htons(targetPort);

    if (connect(backendSocket, (sockaddr*)&backendAddr, sizeof(backendAddr)) == SOCKET_ERROR) {
        std::cerr << "[Proxy] Unexpected failure connecting to " << targetPort << " (Did it crash just now?)\n";

        // Send a real HTTP error to the browser
        const char* error502 = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n<h1>502 Bad Gateway</h1><p>Backend server unexpectedly failed.</p>";
        send(clientSocket, error502, strlen(error502), 0);
        
        closesocket(backendSocket);
        closesocket(clientSocket);
        return;
    }

    while (true) {
        //0. snapshot of exact start time
        // auto start_time = std::chrono::high_resolution_clock::now();

        //1. to recv from caller / recv the browser request
        char buffer[4096] = {0}; // to store incoming msg
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) break;

        // std::cout<<"--raw data recved--\n";
        // std::cout<<buffer<<'\n';
        // std::cout<<"---------------\n";

        // parsing
        HTTPRequest req;
        req.parse(buffer);
        // std::cout<<" "<<req.method<<" "<<req.path<<"\n";

        //3. forward the browser's req to the backend
        if (send(backendSocket, buffer, bytesReceived, 0) == SOCKET_ERROR) break;
        // std::cout << "[Proxy]: forwarded request to backend\n";

        // 4. recv the backend's response AND
        // 5. forward the backend's response to the browser
        char backendBuffer[8192] = {0};
        int backendBytes = recv(backendSocket, backendBuffer, sizeof(backendBuffer), 0);
        if (backendBytes <= 0) break;

        if (send(clientSocket, backendBuffer, backendBytes, 0) == SOCKET_ERROR) break;
        // std::cout << "[Proxy] Relayed " << backendBytes << " bytes\n";

        // auto end_time = std::chrono::high_resolution_clock::now();
        // calculate difference in time
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        // std::cout << "[ACCESS] " << req.method << " " << req.path 
        //           << " -> Backend :" << targetPort 
        //           << " | Relayed " << backendBytes << " bytes"
        //           << " | Latency: " << duration << "ms\n";

        std::string requestStr(buffer, bytesReceived);
        if (requestStr.find("Connection: close") != std::string::npos ||
            requestStr.find("connection: close") != std::string::npos) {
            break;
        }
    }

    shutdown(backendSocket,SD_SEND);
    char dummy1[256];
    while (recv(backendSocket, dummy1, sizeof(dummy1), 0) > 0) {}
    closesocket(backendSocket);

    shutdown(clientSocket,SD_SEND);
    char dummy2[256];
    while (recv(clientSocket, dummy2, sizeof(dummy2), 0) > 0) {}
    closesocket(clientSocket);

    // std::cout<<"[Proxy] connection closed\n\n";
}


void TCPServer::listenForCLient(){
   //accepting client
    // std::cout<<"Waiting for a client to connect...\n";
    SOCKET clientSocket = accept(serverSocket,NULL,NULL);

    if(clientSocket == INVALID_SOCKET){
        if(!running){
            //the stop fn closes the socket
            std::cout << "[System] accept() unblocked for clean shutdown.\n";
            return;
        }
        std::cerr<<"Accept failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    // std::cout<<"A client is connected\n";

    // push the task into ouor thread pool queue
    // the [clientSocket] means "pass this var into the lambda"

    pool.enqueue([clientSocket]{
        handleClient(clientSocket);
    });
}

bool TCPServer::isRunning(){
    return running;
}

bool TCPServer::stop(){
    if(running){
        running=false;
        //accept() is a blocking fn, it pauses program forever until
        // a client connects.
        //By forcibly closing the socket from another thread (the signal handler),
        // accept() will instantly wake up and return INVALID_SOCKET!
        closesocket(serverSocket);
    }
    return true;
}