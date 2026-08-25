#include "server/server.h"
#include<iostream>
#include<windows.h>

TCPServer* global_server = nullptr;

// this fn intercepts ctrl+c
BOOL WINAPI consoleHandler(DWORD signal){
    if(signal==CTRL_C_EVENT){
        std::cout<<"\n[System] Ctrl + C recvd. Initiating clean shutdown..\n";
        if(global_server!=nullptr){
            global_server->stop();
        }
        return true;
    }
    return false;
}
int main(){
    //register ctrl_c handler with wondows

    if(!SetConsoleCtrlHandler(consoleHandler,TRUE)){
        std::cerr<<"Could not set control handler\n";
        return 1;
    }

    TCPServer server("127.0.0.1",8080);
    global_server = &server;


    if(server.start()){
       while(server.isRunning()) server.listenForCLient();
    }else{
        std::cerr<<"Failed to start the server. \n";
    }

    std::cout<<"[System] Server shutdown complete\n";
    return 0;
}

// use this to run :  g++ -o main main.cpp -lws2_32