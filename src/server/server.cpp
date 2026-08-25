#include <thread>
#include "server.h"
#include<iostream>
#include<winsock2.h>
#include<ws2tcpip.h>
#include "../http/request.h"

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
void handleClient(SOCKET clientSocket){
    // to recv from caller
    char buffer[1024]= {0}; // to store incoming msg
    int bytesReceived = recv(clientSocket,buffer, sizeof(buffer),0);

    if(bytesReceived>0){
        std::cout<<"--raw data recved--\n";
        std::cout<<buffer<<'\n';
        std::cout<<"---------------\n";

        // parsing
        HTTPRequest req;
        req.parse(buffer);
    }
    const char* response = "HTTP/1.1 200 OK\r\n" "Content-Type: text/html\r\n" "Content-Length: 46\r\n"   "Connection: close\r\n" "\r\n" "<html><body><h1>Hello World!</h1></body></html>";
    send(clientSocket,response,strlen(response),0);
    shutdown(clientSocket, SD_SEND);
    closesocket(clientSocket);

    std:: cout<<"Client handled and disconnected.\n";

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