#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib") 

#define DEFAULT_PORT "27016" 

bool InitializeWindowsSockets() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}

int main() {
    if (!InitializeWindowsSockets()) {
        return 1;
    }

    SOCKET listenSocket = INVALID_SOCKET;
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(DEFAULT_PORT));  
    serverAddress.sin_addr.s_addr = INADDR_ANY; 

    if (bind(listenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << DEFAULT_PORT << "...\n";

    SOCKET clientSocket = INVALID_SOCKET;
    sockaddr_in clientAddress;
    int clientAddressSize = sizeof(clientAddress);

    clientSocket = accept(listenSocket, (sockaddr*)&clientAddress, &clientAddressSize);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected" << "\n";

    // Receive data from the client (you can modify this for your use case)
    char recvBuffer[512];
    int bytesReceived;
    bytesReceived = recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);
    if (bytesReceived > 0) {
        recvBuffer[bytesReceived] = '\0';  // Null-terminate the received string
        std::cout << "Received message from client: " << recvBuffer << "\n";
    }
    else if (bytesReceived == 0) {
        std::cout << "Connection closed by client.\n";
    }
    else {
        std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
    }

    // Cleanup
    closesocket(clientSocket);
    closesocket(listenSocket);
    WSACleanup();

    return 0;
}
