#ifndef PUBLISHER_H
#define PUBLISHER_H
#include <winsock2.h>
#include <stdbool.h>

typedef struct PublisherMessage
{
	int location;
	char topic[20];
	int message;
	int expirationTime;
}PublisherMessage;

bool InitializeWindowsSockets();
void PrintTopicsMenu(const char** topics, int topicCount);
void ChooseMessage(PublisherMessage* message, const char** topics, int topicCount);

#endif // PUBLISHER_H