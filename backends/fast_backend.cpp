// Fast MULTITHREADED C++ backend server for benchmarking
// Usage: fast_backend.exe <port>
// Spawns a thread per connection for maximum throughput.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

// Pre-built response (shared, read-only — no mutex needed)
char g_response[512];
int g_responseLen = 0;

void handleBackendClient(SOCKET client) {
    int flag = 1;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    char buffer[1024];
    while (true) {
        int bytes = recv(client, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        send(client, g_response, g_responseLen, 0);

        std::string reqStr(buffer, bytes);
        if (reqStr.find("Connection: close") != std::string::npos ||
            reqStr.find("connection: close") != std::string::npos) {
            shutdown(client, SD_SEND);
            char dummy[256];
            while (recv(client, dummy, sizeof(dummy), 0) > 0) {}
            break;
        }
    }

    closesocket(client);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: fast_backend.exe <port>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(serverSocket, (sockaddr*)&addr, sizeof(addr));
    listen(serverSocket, 512);  // Large backlog for high concurrency

    // Build the HTTP response once at startup
    char body[256];
    int bodyLen = snprintf(body, sizeof(body),
        "<html><body><h1>Hello from Backend %d</h1></body></html>", port);
    g_responseLen = snprintf(g_response, sizeof(g_response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: keep-alive\r\n\r\n%s",
        bodyLen, body);

    std::cout << "[Backend] Fast multithreaded C++ backend running on port " << port << "\n";

    while (true) {
        SOCKET client = accept(serverSocket, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        // Spawn a detached thread for each connection — maximum throughput
        std::thread(handleBackendClient, client).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
