#include "Publisher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")


// Initialize Winsock
bool InitializeWindowsSockets() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return false;
    }
    return true;
}


// Generate a random PublisherMessage
void GenerateRandomMessage(PublisherMessage* message, const char** topics, int topicCount) {
    // Generate a random location (0-999)
    message->location = rand() % 1000;

    // Pick a random topic
    strcpy_s(message->topic, topics[rand() % topicCount]);

    // Generate a random message value
    message->message = rand() % 1000;

    // Generate a random expiration time (10-60 seconds)
    message->expirationTime = (rand() % 51) + 10;
}

void PrintTopicsMenu(const char** topics, int topicCount) {
    printf("\nAvailable topics to subscribe:\n");
    for (int i = 0; i < topicCount; i++) {
        printf("%d. %s\n", i + 1, topics[i]);
    }
    printf("%d. Exit\n", topicCount + 1);
}

void ChooseMessage(PublisherMessage *message, const char** topics, int topicCount)
{
    int location, expirationTime;
    int topicChoice, msg;
    printf("*****Publish info*****\n");
    
    printf("Location: ");
    scanf_s("%d", &location);
    
    do {
        PrintTopicsMenu(topics, topicCount);
        printf("\nEnter your topic choice: ");
        scanf_s("%d", &topicChoice);
        if (topicChoice < 1 || topicChoice > topicCount) {
            printf("Invalid choice, please try again.\n");
        }
    } while (topicChoice < 1 || topicChoice > topicCount);

    printf("Message: ");
    scanf_s("%d", &msg);
    
    printf("Exspiration time: ");
    scanf_s("%d", &expirationTime);

    strcpy_s(message->topic, topics[topicChoice-1]);
    message->location = location;
    message->expirationTime = expirationTime;
    message->message = msg;
}

int main()
{
    if (!InitializeWindowsSockets())
    {
        return 1;
    }

    SOCKET connectSocket = INVALID_SOCKET;

    connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(27016);

    if (InetPton(AF_INET, L"127.0.0.1", &serverAddress.sin_addr) <= 0) {
        printf("Invalid address.\n");
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    if (connect(connectSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Unable to connect to server.\n");
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }
    printf("Connected to the server.\n");

    const char* topics[3] = { "Power", "Voltage", "Strength" };

    for (int i = 0; i < 500; i++) {
        PublisherMessage msg;
        GenerateRandomMessage(&msg, topics, 3);

        // Send the message to the server
        int bytesSent = send(connectSocket, (char*)&msg, sizeof(PublisherMessage), 0);
        if (bytesSent == SOCKET_ERROR) {
            printf("Failed to send message %d to server.\n", i + 1);
        }
        else {
            printf("Message %d sent: Location=%d, Topic=%s, Value=%d, Expiration=%d\n",
                i + 1, msg.location, msg.topic, msg.message, msg.expirationTime);
        }

        // Sleep for 2 seconds before sending the next message
        Sleep(2000);
    }

    printf("All messages sent. Press any key to close.\n");
    _getch();

    // Cleanup
    closesocket(connectSocket);
    WSACleanup();

    return 0;
}

