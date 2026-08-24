#include "server/server.h"
#include<iostream>

int main(){
    TCPServer server("127.0.0.1",8080);

    if(server.start()){
        server.listenForCLient();
    }else{
        std::cerr<<"Failed to start the server. \n";
    }
    return 0;
}

// use this to run :  g++ -o main main.cpp -lws2_32