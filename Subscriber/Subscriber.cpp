#include "Subscriber.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>
#include <iostream>


// Initialize Winsock
bool InitializeWindowsSockets() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return false;
    }
    return true;
}

//Print funkcija za meni
void PrintTopicsMenu(const char** topics, int topicCount) {
    printf("\nAvailable topics to subscribe:\n");
    for (int i = 0; i < topicCount; i++) {
        printf("%d. %s\n", i + 1, topics[i]);
    }
    printf("%d. Exit\n", topicCount + 1);
}

//Biranje teme i postavljanje lokacije
void ChooseSubscription(SubscriberData* subscriber, const char** topics, int topicCount) {
    int topicChoice;
    do {
        PrintTopicsMenu(topics, topicCount);
        printf("\nEnter your topic choice: ");
        scanf_s("%d", &topicChoice);
        if (topicChoice < 1 || topicChoice > topicCount) {
            printf("Invalid choice, please try again.\n");
        }
    } while (topicChoice < 1 || topicChoice > topicCount);

    // Set the chosen topic
    strcpy_s(subscriber->subscription.topic, topics[topicChoice - 1]);

    // Set a random location (0-999)
    //TO DO: Make a location function
    subscriber->subscription.location = rand() % 1000;

    printf("\nSubscribed to Topic: %s, Location: %d\n",
        subscriber->subscription.topic,
        subscriber->subscription.location);
}

//Slanje socket-a i subskripcije serveru
void SendSubscriptionToServer(SubscriberData* subscriber) {
    int bytesSent = send(subscriber->connectSocket,
        (char*)subscriber,
        sizeof(SubscriberData), // Send the whole SubscriberData structure
        0);
    if (bytesSent == SOCKET_ERROR) {
        printf("Failed to send subscription to server.\n");
    }
    else {
        printf("Subscription sent to server successfully.\n");
    }
}

//Thread koji sluzi za primanje poruka od servera
DWORD WINAPI ThreadReceiveMessages(LPVOID lpParam) {
    SubscriberData* subscriber = (SubscriberData*)lpParam;
    char buffer[100]; // Ensure this is large enough for your formatted message
    int result;
    printf("Waiting for messages...\n");
    while (1) {
        result = recv(subscriber->connectSocket, buffer, sizeof(buffer) - 1, 0);
        if (result > 0) {
            buffer[result] = '\0'; // Null-terminate the string
            printf("\n[Server]: %s\n", buffer);
        }
        else if (result == 0) {
            printf("Connection closed by server.\n");
            break;
        }
        else {
            printf("recv failed with error: %d\n", WSAGetLastError());
            break;
        }
    }

    return 0;
}

//Pokretanje Main funkcije
int main() {
    if (!InitializeWindowsSockets()) {
        return 1;
    }

    const char* topics[3] = { "Power", "Voltage", "Strength" };
    SubscriberData subscriber;
    SOCKET connectSocket = INVALID_SOCKET;

    srand((unsigned int)time(NULL)); // Seed for random location generation

    connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(27017);

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

    // Initialize subscriber data
    subscriber.connectSocket = connectSocket;
    ChooseSubscription(&subscriber, topics, 3);

    // Send subscription to the server
    SendSubscriptionToServer(&subscriber);

    // Start the thread for receiving messages
    HANDLE threadHandle;
    DWORD threadId;
    threadHandle = CreateThread(NULL, 0, ThreadReceiveMessages, &subscriber, 0, &threadId);
    if (threadHandle == NULL) {
        printf("Failed to create receive thread. Error: %d\n", GetLastError());
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }



    // Wait for ESC to exit
    while (1) {
        if (_kbhit() && _getch() == 27) { // ESC key
            break;
        }
    }

    printf("Closing subscriber...\n");

    // Clean up
    closesocket(connectSocket);
    WSACleanup();
    CloseHandle(threadHandle);

    return 0;
}