#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H
#include <winsock2.h>
#include <stdbool.h>

#define TOPIC_COUNT 3
#define TOPIC_POWER "Power"
#define TOPIC_VOLTAGE "Voltage"
#define TOPIC_STRENGTH "Strength"

typedef struct SubscribedTo {
    char topic[20];
    int location;
} SubscribedTo;

typedef struct {
    SOCKET connectSocket;
    SubscribedTo subscription;
} SubscriberData;


bool InitializeWindowsSockets();
void PrintTopicsMenu(const char** topics, int topicCount);
void ChooseSubscription(SubscriberData* subscriber, const char** topics, int topicCount);
void SendSubscriptionToServer(SubscriberData* subscriber);
DWORD WINAPI ThreadReceiveMessages(LPVOID lpParam);

#endif // SUBSCRIBER_H
