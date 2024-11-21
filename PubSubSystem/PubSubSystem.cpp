#include "PubSubService.h"
#pragma comment(lib, "ws2_32.lib") 



bool InitializeWindowsSockets() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}


// Function to handle a subscriber connection
void HandleSubscriber(SOCKET clientSocket) {
    SubscribedTo subscriberData;
    int bytesReceived;

    bytesReceived = recv(clientSocket, (char*)&subscriberData, sizeof(SubscribedTo), 0);
    if (bytesReceived > 0) {
        if (bytesReceived == sizeof(SubscribedTo)) {
            std::cout << "Subscriber connected:\n";
            std::cout << "  Topic: " << subscriberData.topic << "\n";
            std::cout << "  Location: " << subscriberData.location << "\n";
        }
        else {
            std::cerr << "Partial data received from subscriber. Expected: "
                << sizeof(SubscribedTo) << ", Received: " << bytesReceived << "\n";
        }
    }
    else {
        std::cerr << "Failed to receive subscriber data.\n";
    }
}


void HandlePublisher(SOCKET clientSocket) {
    PublisherMessage receivedMessage;
    int bytesReceived;

    while (true) {
        bytesReceived = recv(clientSocket, (char*)&receivedMessage, sizeof(PublisherMessage), 0);
        if (bytesReceived > 0) {
            if (bytesReceived == sizeof(PublisherMessage)) {
                std::cout << "Received message:\n";
                std::cout << "  Location: " << receivedMessage.location << "\n";
                std::cout << "  Topic: " << receivedMessage.topic << "\n";
                std::cout << "  Value: " << receivedMessage.message << "\n";
                std::cout << "  Expiration Time: " << receivedMessage.expirationTime << " seconds\n";

                // Forward message to matching subscribers
                std::lock_guard<std::mutex> lock(subscribersMutex);
                for (const auto& subscriber : subscribers) {
                    if ((strcmp(subscriber.subscription.topic, receivedMessage.topic) == 0) &&
                        (subscriber.subscription.location == receivedMessage.location)) {
                        send(subscriber.connectSocket, (char*)&receivedMessage, sizeof(receivedMessage), 0);
                    }
                }
            }
        }
        else if (bytesReceived == 0) {
            std::cout << "Publisher disconnected.\n";
            break;
        }
        else {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    closesocket(clientSocket);
}


int main() {
    if (!InitializeWindowsSockets()) {
        return 1;
    }

    SOCKET publisherListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET subscriberListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (publisherListenSocket == INVALID_SOCKET || subscriberListenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Bind publisher socket
    serverAddress.sin_port = htons(atoi(DEFAULT_PORT));
    if (bind(publisherListenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Publisher bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(publisherListenSocket);
        WSACleanup();
        return 1;
    }

    // Bind subscriber socket
    serverAddress.sin_port = htons(atoi(SUBSCRIBER_PORT));
    if (bind(subscriberListenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Subscriber bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(subscriberListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(publisherListenSocket, SOMAXCONN) == SOCKET_ERROR || listen(subscriberListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(publisherListenSocket);
        closesocket(subscriberListenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening for publishers on port " << DEFAULT_PORT << "...\n";
    std::cout << "Server is listening for subscribers on port " << SUBSCRIBER_PORT << "...\n";

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(publisherListenSocket, &readSet);
        FD_SET(subscriberListenSocket, &readSet);

        timeval timeout = { 1, 0 };  // 1-second timeout
        int selectResult = select(0, &readSet, NULL, NULL, &timeout);

        if (selectResult > 0) {
            if (FD_ISSET(publisherListenSocket, &readSet)) {
                SOCKET publisherSocket = accept(publisherListenSocket, NULL, NULL);
                if (publisherSocket != INVALID_SOCKET) {
                    std::thread publisherThread(HandlePublisher, publisherSocket);
                    publisherThread.detach();
                }
            }

            if (FD_ISSET(subscriberListenSocket, &readSet)) {
                SOCKET subscriberSocket = accept(subscriberListenSocket, NULL, NULL);
                if (subscriberSocket != INVALID_SOCKET) {
                    std::thread subscriberThread(HandleSubscriber, subscriberSocket);
                    subscriberThread.detach();
                }
            }
        }
    }

    closesocket(publisherListenSocket);
    closesocket(subscriberListenSocket);
    WSACleanup();

    return 0;
}