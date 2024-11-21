#pragma once
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

#define DEFAULT_PORT "27016" 
#define SUBSCRIBER_PORT "27017"
// PublisherMessage structure
struct PublisherMessage {
    int location;
    char topic[20];
    int message;
    int expirationTime;
};

// Circular buffer structure for PublisherMessage
struct CircularBuffer {
    PublisherMessage buffer[100]; // Fixed size buffer for simplicity
    int head;                     // Points to the next position to write
    int tail;                     // Points to the next position to read
    int size;                     // Number of messages currently in the buffer
    CRITICAL_SECTION cs;          // Critical section for thread safety
};

// Add a message to the circular buffer
bool AddToCircularBuffer(CircularBuffer* cb, PublisherMessage* message) {
    EnterCriticalSection(&cb->cs);

    // Check if buffer is full
    if (cb->size == 100) {
        LeaveCriticalSection(&cb->cs);
        return false; // Buffer full
    }

    cb->buffer[cb->head] = *message;
    cb->head = (cb->head + 1) % 100;
    cb->size++;

    LeaveCriticalSection(&cb->cs);
    return true;
}

// Get a message from the circular buffer
bool GetFromCircularBuffer(CircularBuffer* cb, PublisherMessage* message) {
    EnterCriticalSection(&cb->cs);

    // Check if buffer is empty
    if (cb->size == 0) {
        LeaveCriticalSection(&cb->cs);
        return false; // Buffer empty
    }

    *message = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % 100;
    cb->size--;

    LeaveCriticalSection(&cb->cs);
    return true;
}



//SUBSCRIBER

struct SubscribedTo {
    char topic[20];
    int location;
};

struct SubscriberData {
    SOCKET connectSocket;
    SubscribedTo subscription;
};

std::vector<SubscriberData> subscribers;
std::mutex subscribersMutex;