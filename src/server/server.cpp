#include <thread>
#include "server.h"
#include<iostream>
#include<winsock2.h>
#include<ws2tcpip.h>
#include "../http/request.h"
#include<string>
#include<vector>
#include<mutex>

TCPServer::TCPServer(std::string ip_address, int port):ip_address(ip_address), port(port),pool(8){
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

std::vector<int>backends = {9001,9002,9003}; // list of backend server
int current_backend_index=0;
std::mutex rr_mutex; // protects the current_backend_index


void handleClient(SOCKET clientSocket){
    //1. to recv from caller / recv the browser request
    char buffer[4096]= {0}; // to store incoming msg
    int bytesReceived = recv(clientSocket,buffer, sizeof(buffer),0);

     if(bytesReceived<=0){
        closesocket(clientSocket);
        return;
     }
    
        std::cout<<"--raw data recved--\n";
        std::cout<<buffer<<'\n';
        std::cout<<"---------------\n";

        // parsing
        HTTPRequest req;
        req.parse(buffer);
        std::cout<<" "<<req.method<<" "<<req.path<<"\n";

    // 2. connect to the backend (load balancer and Fault tolerant)
    SOCKET backendSocket = INVALID_SOCKET;
    int retries = backends.size() ; // try each backend exactly once
    bool connected = false;
    int targetPort = 0;

    while(retries>0 && !connected){
        // safely get the next prot using Round-Robin mutex
        {
            std::unique_lock<std::mutex>lock(rr_mutex);
            targetPort = backends[current_backend_index];
            current_backend_index = (current_backend_index+1)%backends.size();
            //mutex release here
        }
        backendSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

        sockaddr_in backendAddr;
        backendAddr.sin_family = AF_INET;
        backendAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        backendAddr.sin_port = htons(targetPort);

        std::cout<<"[Proxy] Attempting to connect backend : "<<targetPort<<"...\n";

        if(connect(backendSocket, (sockaddr*)&backendAddr, sizeof(backendAddr))!=SOCKET_ERROR){
            connected=true;
            std::cout<<"[Proxy] suuccessfully connected to backend "<<targetPort<<"\n";
        }else{
            std::cerr<<"[Proxy] Backend: "<<targetPort<<" is down! Trying the next";
            closesocket(backendSocket);
            retries--;
        }
    }

    if(!connected){
        std::cerr<<"[Proxy] All backends are DOWN! Dropping client.\n";
        closesocket(clientSocket);
        return;
    }

    //3. forward the browser's req to the backend
    //AND  Modify the request: tell backend to close connection after responding


    std::string request(buffer, bytesReceived);
    size_t pos = request.find("Connection: keep-alive");
    if (pos != std::string::npos) {
        request.replace(pos, 22, "Connection: close");
    }
    send(backendSocket, request.c_str(), request.size(), 0);
    std::cout << "[Proxy]: forwarded request to backend\n";

    // 4. recv the backend's response AND
    // 5. forward the backend's response to the browser
    char backendBuffer[8192] = {0};
    // Keep reading chunks from backend until it's done sending
    int backendBytes;
    while ((backendBytes = recv(backendSocket, backendBuffer, sizeof(backendBuffer), 0)) > 0) {
        send(clientSocket, backendBuffer, backendBytes, 0);
        std::cout << "[Proxy] Relayed " << backendBytes << " bytes\n";
    }
    std::cout << "[Proxy] Backend finished sending.\n";

    shutdown(backendSocket,SD_SEND);
    closesocket(backendSocket);
    shutdown(clientSocket,SD_SEND);
    closesocket(clientSocket);
    std::cout<<"[Proxy] connection closed\n\n";
}


void TCPServer::listenForCLient(){
   //accepting client
    std::cout<<"Waiting for a client to connect...\n";
    SOCKET clientSocket = accept(serverSocket,NULL,NULL);

    if(clientSocket == INVALID_SOCKET){
        std::cerr<<"Accept failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout<<"A client is connected\n";

    // push the task into ouor thread pool queue
    // the [clientSocket] means "pass this var into the lambda"

    pool.enqueue([clientSocket]{
        handleClient(clientSocket);
    });
}