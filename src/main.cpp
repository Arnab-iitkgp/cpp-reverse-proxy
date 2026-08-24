#include<iostream>
#include<winsock2.h>
#include<ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

int main(){
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { //D1-what it is
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    std::cout<<"Networking initialised succesfully"<<'\n';

    //scocket lifecycle
    //HERE AF_INFET means we re using ipv4 adresses, SOCK_STREAM = we are using TCP.
    SOCKET serverSocket  = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(serverSocket== INVALID_SOCKET){
        std::cerr<<"Error creating socket. "<<std::endl;
        WSACleanup();
        return 1;
    }
    std::cout<<"socket created!\n";

    // assigning the ip and port to the socket -- bind
    sockaddr_in serverAddress;
    serverAddress.sin_family =AF_INET; //ipv4
    serverAddress.sin_addr.s_addr=INADDR_ANY; // listen on any ip my pc has
    serverAddress.sin_port = htons(8080); // convert to network byte order
    
    //bind
    if(bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress))==SOCKET_ERROR){
        std::cerr<<"Bind failed! port 8080 might already be in use\n";
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

    std::cout<<"Server is listening on PORT 8080\n";

    //accepting client
    std::cout<<"Waiting for a client to connect...\n";
    SOCKET clientSocket = accept(serverSocket,NULL,NULL);

    if(clientSocket == INVALID_SOCKET){
        std::cerr<<"Accept failed\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout<<"A client is connected\n";

    // listen to the caller (recv);

    char buffer[1024] ={0}; // to store the incoming msg in chars;
    int bytesReceived  = recv(clientSocket, buffer,sizeof(buffer),0);

    if(bytesReceived>0){
        std::cout<<"--data recved--\n";
        std::cout<<buffer<<'\n';
        std::cout<<"---------------\n";
    }

    // talk back -- send;
    // we send a tiny raw http response so the browser can understand it;

    const char* response = "HTTP/1.1 200 OK\r\n Content-Length: 13\r\n\r\nHello, World\n";
    send(clientSocket,response,strlen(response),0);

    closesocket(clientSocket);

    //cleanup before exiting
    WSACleanup();

    return 0;
}

// use this to run :  g++ -o main main.cpp -lws2_32