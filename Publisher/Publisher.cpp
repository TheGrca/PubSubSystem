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


// Generisanje random lokacije, topic-a, poruke i expiration time
void GenerateRandomMessage(PublisherMessage* message, const char** topics, int topicCount) {
    // Generate a random location (0-999)
    message->location = rand() % 100;

    // Pick a random topic
    strcpy_s(message->topic, topics[rand() % topicCount]);

    // Generate a random message value
    message->message = rand() % 1000;

    // Generate a random expiration time (10-60 seconds)
    message->expirationTime = (rand() % 10) + 10;
}

//Main funkcija
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
    serverAddress.sin_port = htons(27019);

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

    
    //For petlja koja trenutno salje 500 poruka sa sleepom od 2 sekunde
    for (int i = 0; i < 10000; i++) {
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
        //Sleep(2000);
    }

    /* TEST FUNCTION FOR REMOVING FROM HEAP
    PublisherMessage messages[3] = {
    {130, "Voltage", 30, 30}, 
    {105, "Voltage", 30, 5},  
    {115, "Voltage", 30, 15}  
    };

    for (int i = 0; i < 3; i++) {
        int bytesSent = send(connectSocket, (char*)&messages[i], sizeof(PublisherMessage), 0);
        if (bytesSent == SOCKET_ERROR) {
            printf("Failed to send message %d to server.\n", i + 1);
        }
        else {
            printf("Message %d sent: Location=%d, Topic=%s, Value=%d, Expiration=%d\n",
                i + 1, messages[i].location, messages[i].topic, messages[i].message, messages[i].expirationTime);
        }
        Sleep(2000);
    }
    */
    printf("All messages sent. Press any key to close.\n");
    _getch();

    // Cleanup
    closesocket(connectSocket);
    WSACleanup();

    return 0;
}

