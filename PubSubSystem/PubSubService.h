#pragma once
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

#define DEFAULT_PORT "27019" 
#define SUBSCRIBER_PORT "27020"
#define BUFFER_SIZE 100
#define SUBSCRIBER_BUFFER_SIZE 100
#define THREAD_POOL 4

// PublisherMessage structure
struct PublisherMessage {
    int location;
    char topic[20];
    int message;
    int expirationTime;
};



typedef struct {
    PublisherMessage buffer[BUFFER_SIZE];
    int head; // Write position
    int tail; // Read position
    int size; // Number of messages in buffer
    CRITICAL_SECTION cs;
} CircularBuffer;

typedef struct {
    PublisherMessage* heap[BUFFER_SIZE];
    int size;
    CRITICAL_SECTION cs;
} ProcessedHeap;

void InitializeCircularBuffer(CircularBuffer* cb);
bool AddToCircularBuffer(CircularBuffer* cb, PublisherMessage* message);

//SUBSCRIBER

struct SubscribedTo {
    char topic[20];
    int location;
};

struct SubscriberData {
    SOCKET connectSocket;
    SubscribedTo subscription;
};

typedef struct HashmapEntry {
    SubscriberData* subscribers[10];
    int subscriberCount;
} HashmapEntry;

HashmapEntry locationSubscribers[1000];  // Keys 0-999
HashmapEntry topicSubscribers[3];        // Keys "Power", "Voltage", "Strength"

void InitializeHashmaps();
void AddSubscriberToLocation(int location, SubscriberData* subscriber);
void AddSubscriberToTopic(const char* topic, SubscriberData* subscriber);
bool GetFromCircularBuffer(CircularBuffer* cb, PublisherMessage* message);
void InitializeHeap(ProcessedHeap* pq);
bool AddToHeap(ProcessedHeap* heap, PublisherMessage* message);
void RemoveExpiredFromHeap(ProcessedHeap* heap);
PublisherMessage RemoveFromHeap(ProcessedHeap* pq);
PublisherMessage peek(ProcessedHeap* pq);
void Swap(PublisherMessage* a, PublisherMessage* b);
void HeapifyUp(ProcessedHeap* pq, int index);
void HeapifyDown(ProcessedHeap* pq, int index);
time_t ParseTime(int seconds);
