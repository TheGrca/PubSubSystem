#include "PubSubService.h"
#pragma comment(lib, "ws2_32.lib") 


CircularBuffer cb;
ProcessedHeap pq;
HANDLE emptySemaphore;
HANDLE fullSemaphore;


//Inicijalizacija socketa
bool InitializeWindowsSockets() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}


// Hendluje primanje subskripcije i socketa od subscribera
void HandleSubscriber(SOCKET clientSocket) {
    SubscriberData* subscriberData = (SubscriberData*)malloc(sizeof(SubscriberData));
    int bytesReceived;

    // Receive subscriber data
    bytesReceived = recv(clientSocket, (char*)subscriberData, sizeof(SubscriberData), 0);
    if (bytesReceived == sizeof(SubscriberData)) {
        // Set the connectSocket to the accepted socket
        subscriberData->connectSocket = clientSocket;

        // Add subscriber to both location and topic hashmaps
        AddSubscriberToLocation(subscriberData->subscription.location, subscriberData);
        AddSubscriberToTopic(subscriberData->subscription.topic, subscriberData);

        printf("Subscriber connected: Location %d, Topic %s\n",
            subscriberData->subscription.location, subscriberData->subscription.topic);

        // Send a confirmation back to the subscriber
        const char* confirmationMessage = "Subscription successful.";
        send(clientSocket, confirmationMessage, strlen(confirmationMessage), 0);
        for (int i = 0; i < pq.size; i++)
        {
            printf("\nTopic u heapu: %s", pq.heap[i]->topic);
            if (strcmp(subscriberData->subscription.topic, pq.heap[i]->topic) == 0)
            {
                char formattedMessage[100];
                snprintf(formattedMessage, sizeof(formattedMessage),
                    "Location: %d, Topic: %s, Message: %d",
                    pq.heap[i]->location, pq.heap[i]->topic, pq.heap[i]->message);

                EnterCriticalSection(&pq.cs);
                // Now send the formatted message
                int bytesSent = send(subscriberData->connectSocket, formattedMessage, strlen(formattedMessage), 0);
                LeaveCriticalSection(&pq.cs);
                if (bytesSent == SOCKET_ERROR) {
                    printf("Failed to send message to subscriber at location: %d, Error: %d\n",
                        pq.heap[i]->location, WSAGetLastError());
                }
                else {
                    printf("Sent message to subscriber: %s", formattedMessage);
                }
                
            }
        }
        printf("***************************************************");
    }
    else {
        printf("Failed to receive subscriber data. Expected size: %zu, Received size: %d\n",
            sizeof(SubscriberData), bytesReceived);
    }

    // Free the subscriberData later when the subscriber disconnects
}


//Hendluje primanje poruke od publishera
void HandlePublisher(SOCKET clientSocket) {
    PublisherMessage message;
    int bytesReceived;

    while (true) {
        bytesReceived = recv(clientSocket, (char*)&message, sizeof(PublisherMessage), 0);
        if (bytesReceived > 0) {
            if (bytesReceived == sizeof(PublisherMessage)) {
                WaitForSingleObject(emptySemaphore, INFINITE); // Wait for space in the buffer

                if (AddToCircularBuffer(&cb, &message)) {
                    std::cout << "Added message to buffer:\n";
                    std::cout << "  Location: " << message.location << "\n";
                    std::cout << "  Topic: " << message.topic << "\n";
                    std::cout << "  Value: " << message.message << "\n";
                    std::cout << "  Expiration Time: " << message.expirationTime << "\n";
                }

                ReleaseSemaphore(fullSemaphore, 1, NULL); // Signal a new message in buffer
            }
        }
        else {
            std::cout << "Publisher disconnected.\n";
            break;
        }
    }

    closesocket(clientSocket);
}

//Inicijalizacija kruznog buffera
void InitializeCircularBuffer(CircularBuffer* cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->size = 0;
    InitializeCriticalSection(&cb->cs);
}

//Dodavanje poruke od publishera u buffer
bool AddToCircularBuffer(CircularBuffer* cb, PublisherMessage* message) {
    EnterCriticalSection(&cb->cs);

    if (cb->size == BUFFER_SIZE) {
        LeaveCriticalSection(&cb->cs);
        return false; // Buffer full
    }

    cb->buffer[cb->head] = *message;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
    cb->size++;

    LeaveCriticalSection(&cb->cs);
    return true;
}

//Thread za procesiranje i slanje poruke subscriberu od publishera
DWORD WINAPI ConsumerThread(LPVOID param) {
    ProcessedHeap* heap = (ProcessedHeap*)param;
    while (true) {
        WaitForSingleObject(fullSemaphore, INFINITE);

        EnterCriticalSection(&cb.cs);

        // Check if there are messages to process
        if (cb.size > 0) {
            // Retrieve the message from the circular buffer
            PublisherMessage message = cb.buffer[cb.tail];
            message.expirationTime = ParseTime(message.expirationTime);
            AddToHeap(&pq, &message);
            cb.tail = (cb.tail + 1) % BUFFER_SIZE;
            cb.size--;


            ReleaseSemaphore(emptySemaphore, 1, NULL);

            // Forward the message to all matching subscribers based on location
            if (message.location >= 0 && message.location < 1000) {
                HashmapEntry* locationEntry = &locationSubscribers[message.location];
                for (int i = 0; i < locationEntry->subscriberCount; i++) {
                    SubscriberData* subscriber = locationEntry->subscribers[i];

                    // Check if the socket is valid
                    if (subscriber->connectSocket != INVALID_SOCKET) {
                        char formattedMessage[100];
                        snprintf(formattedMessage, sizeof(formattedMessage),
                            "Location: %d, Topic: %s, Message: %d",
                            message.location, message.topic, message.message);

                        // Now send the formatted message
                        int bytesSent = send(subscriber->connectSocket, formattedMessage, strlen(formattedMessage), 0);
                        if (bytesSent == SOCKET_ERROR) {
                            printf("Failed to send message to subscriber at location: %d, Error: %d\n",
                                message.location, WSAGetLastError());
                        }
                        else {
                            printf("  Forwarded to subscriber at location: %d\n", message.location);
                        }
                    }
                    else {
                        printf("Invalid socket for subscriber at location: %d\n", message.location);
                    }
                }
            }

            // Determine the topic index
            int topicIndex = -1;
            if (strcmp(message.topic, "Power") == 0) topicIndex = 0;
            else if (strcmp(message.topic, "Voltage") == 0) topicIndex = 1;
            else if (strcmp(message.topic, "Strength") == 0) topicIndex = 2;

            // Forward the message to all matching subscribers based on topic
            if (topicIndex >= 0) {
                HashmapEntry* topicEntry = &topicSubscribers[topicIndex];
                for (int i = 0; i < topicEntry->subscriberCount; i++) {
                    SubscriberData* subscriber = topicEntry->subscribers[i];

                    // Check if the socket is valid
                    if (subscriber->connectSocket != INVALID_SOCKET) {
                        char formattedMessage[100];
                        snprintf(formattedMessage, sizeof(formattedMessage),
                            "Location: %d, Topic: %s, Message: %d",
                            message.location, message.topic, message.message);

                        // Now send the formatted message
                        int bytesSent = send(subscriber->connectSocket, formattedMessage, strlen(formattedMessage), 0);
                        if (bytesSent == SOCKET_ERROR) {
                            printf("Failed to send message to subscriber for topic: %d, Error: %d\n",
                                message.location, WSAGetLastError());
                        }
                        else {
                            printf("  Forwarded to subscriber for topic: %s\n", message.topic);
                        }
                    }
                    else {
                        printf("Invalid socket for subscriber for topic: %s\n", message.topic);
                    }
                }
            }
            LeaveCriticalSection(&cb.cs);
        }
        else {
            LeaveCriticalSection(&cb.cs);
        }
       // Sleep(100); // Prevent tight looping
    }

    return 0;
}


//Inicijalizacija HashMapa za subscribere
void InitializeHashmaps() {
    memset(locationSubscribers, 0, sizeof(locationSubscribers));
    memset(topicSubscribers, 0, sizeof(topicSubscribers));
}
//Dodavanje subscribera u hashmapu za lokacije
void AddSubscriberToLocation(int location, SubscriberData* subscriber) {
    HashmapEntry* entry = &locationSubscribers[location];
    if (entry->subscriberCount < 10) {
        entry->subscribers[entry->subscriberCount++] = subscriber;
    }
}
//Dodavanje subscribera u hashmapu za topic
void AddSubscriberToTopic(const char* topic, SubscriberData* subscriber) {
    int index = (strcmp(topic, "Power") == 0) ? 0 : (strcmp(topic, "Voltage") == 0) ? 1 : 2;
    HashmapEntry* entry = &topicSubscribers[index];
    if (entry->subscriberCount < 10) {
        entry->subscribers[entry->subscriberCount++] = subscriber;
    }
}
// Thread koji je stalno aktivan i trazi konekcije od subscribera
DWORD WINAPI SubscriberListenerThread(LPVOID param) {
    SOCKET subscriberListenSocket = *(SOCKET*)param;
    while (true) {
        SOCKET subscriberSocket = accept(subscriberListenSocket, NULL, NULL);
        if (subscriberSocket != INVALID_SOCKET) {
            std::cout << "Subscriber connected.\n";
            std::thread subscriberThread(HandleSubscriber, subscriberSocket);
            subscriberThread.detach();
        }
    }
    return 0;
}

bool GetFromCircularBuffer(CircularBuffer* cb, PublisherMessage* message) {
    EnterCriticalSection(&cb->cs);

    if (cb->size == 0) {
        LeaveCriticalSection(&cb->cs);
        return false; // Buffer empty
    }

    *message = cb->buffer[cb->tail];
    AddToHeap(&pq, message);
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    cb->size--;

    LeaveCriticalSection(&cb->cs);
    return true;
}

void InitializeHeap(ProcessedHeap* pq)
{
    for (int i = 0; i < 10000; i++)
    {
        pq->heap[i] = NULL;
    }
    pq->size = 0;
    InitializeCriticalSection(&pq->cs);
}
//Dodavanje poruke od publishera u heap
bool AddToHeap(ProcessedHeap* heap, PublisherMessage* message) {
    //EnterCriticalSection(&heap->cs);

    if (heap->size == 10000) {
        //LeaveCriticalSection(&heap->cs);
        return false; // Heap full
    }
    PublisherMessage* newMessage = (PublisherMessage*)malloc(sizeof(PublisherMessage));
    if (!newMessage) {
        printf("Memory allocation failed for new heap element.\n");
        return false;
    }
    *newMessage = *message;

    // Add to the end of the heap
    heap->heap[heap->size] = newMessage;
    int currentIndex = heap->size;
    heap->size++;

    //HeapifyUp(&pq, currentIndex);
    // Bubble-up to maintain heap order (min-heap or max-heap)
    
    while (currentIndex > 0) {
        int parentIndex = (currentIndex - 1) / 2;
        if (heap->heap[parentIndex]->expirationTime > heap->heap[currentIndex]->expirationTime) {
            // Swap with parent
            PublisherMessage* temp = heap->heap[parentIndex];
            heap->heap[parentIndex] = heap->heap[currentIndex];
            heap->heap[currentIndex] = temp;
            currentIndex = parentIndex;
        }
        else {
            break; // Heap property satisfied
        }
    }
    
    printf("Dodat %d u heap %s\n", newMessage->location, newMessage->topic);
    //LeaveCriticalSection(&heap->cs);
    return true;
}

//Izbacivanje poruke iz heapa od publishera ako je isteklo vrijeme
void RemoveExpiredFromHeap(ProcessedHeap* heap) {
    EnterCriticalSection(&heap->cs);

    if (heap->size > 0) {
        time_t currentTime = time(0);
        if (currentTime >= peek(heap).expirationTime) {
            PublisherMessage removedMessage = RemoveFromHeap(heap);
            printf("Message removed: Location: %d, Topic: %s\n",
                removedMessage.location, removedMessage.topic);
        }
    }

    LeaveCriticalSection(&heap->cs);
}

void FreeHeap(ProcessedHeap* heap) {
    for (int i = 0; i < heap->size; i++) {
        free(heap->heap[i]); // Free each allocated message
    }
    heap->size = 0; // Reset the heap size
}

PublisherMessage RemoveFromHeap(ProcessedHeap* pq)
{
    PublisherMessage ERROR_MESSAGE = { -1, "", -1, NULL };
    if (!pq->size) {
        printf("Priority queue is empty\n");
        return ERROR_MESSAGE;
    }

    PublisherMessage item = *pq->heap[0];
    pq->heap[0] = pq->heap[--pq->size];
    HeapifyDown(pq, 0);

    return item;
}

PublisherMessage peek(ProcessedHeap* pq)
{
    PublisherMessage ERROR_MESSAGE = { -1, "", -1, NULL };
    if (!pq->size) {
        printf("Priority queue is empty\n");
        return ERROR_MESSAGE;
    }
    return *pq->heap[0];
}

//Menjanje dva elementa u heap-u
void Swap(PublisherMessage* a, PublisherMessage* b)
{
    PublisherMessage temp = *a;
    *a = *b;
    *b = temp;
}

//Odrzavanje heap property-ja pri dodavanju
void HeapifyUp(ProcessedHeap* pq, int index)
{
    if (index
        && pq->heap[(index - 1) / 2]->expirationTime > pq->heap[index]->expirationTime) {
        Swap(pq->heap[(index - 1) / 2],
            pq->heap[index]);
        HeapifyUp(pq, (index - 1) / 2);
    }
}

//Odrzavanje heap property-ja pri dodavanju pri brisanju
void HeapifyDown(ProcessedHeap* pq, int index)
{
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < pq->size
        && pq->heap[left]->expirationTime < pq->heap[smallest]->expirationTime)
        smallest = left;

    if (right < pq->size
        && pq->heap[right]->expirationTime < pq->heap[smallest]->expirationTime)
        smallest = right;

    if (smallest != index) {
        Swap(pq->heap[index], pq->heap[smallest]);
        HeapifyDown(pq, smallest);
    }
}

time_t ParseTime(int seconds)
{
    time_t now = time(0);
    return now + seconds;
}


DWORD WINAPI MonitorHeapThread(LPVOID param) {
    ProcessedHeap* heap = (ProcessedHeap*)param;

    while (1) {
        Sleep(10); // Sleep for 0.2 seconds

        EnterCriticalSection(&heap->cs);

        if (heap->size > 0) {
            time_t currentTime = time(0);
            PublisherMessage topMessage = peek(heap);
            if (currentTime >= topMessage.expirationTime) {
                PublisherMessage removedMessage = RemoveFromHeap(heap);
                printf("Message removed: Location: %d, Topic: %s\n",
                    removedMessage.location, removedMessage.topic);
            }
        }

        LeaveCriticalSection(&heap->cs);
    }
    return 0;
}


int main() {
    if (!InitializeWindowsSockets()) {
        return 1;
    }
    sockaddr_in publisherAddress;
    publisherAddress.sin_family = AF_INET;
    publisherAddress.sin_addr.s_addr = INADDR_ANY;
    publisherAddress.sin_port = htons(atoi(DEFAULT_PORT));

    sockaddr_in subscriberAddress;
    subscriberAddress.sin_family = AF_INET;
    subscriberAddress.sin_addr.s_addr = INADDR_ANY;
    subscriberAddress.sin_port = htons(atoi(SUBSCRIBER_PORT));

    InitializeHeap(&pq);
    InitializeCircularBuffer(&cb);
    emptySemaphore = CreateSemaphore(NULL, BUFFER_SIZE, BUFFER_SIZE, NULL);
    fullSemaphore = CreateSemaphore(NULL, 0, BUFFER_SIZE, NULL);
    //Aktiviranje thread-a koji salje poruke subscriberima
    HANDLE consumerThreads[THREAD_POOL];
    for (int i = 0; i < THREAD_POOL; i++) {
        consumerThreads[i] = CreateThread(NULL, 0, ConsumerThread, &pq, 0, NULL);
        if (consumerThreads[i] == NULL) {
            std::cerr << "Failed to create consumer thread.\n";
            WSACleanup();
            return 1;
        }
    }


    //Kreiranje socketa
    SOCKET publisherListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET subscriberListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    //Pitamo za gresku kod socketa
    if (publisherListenSocket == INVALID_SOCKET || subscriberListenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind publisher socket
    if (bind(publisherListenSocket, (sockaddr*)&publisherAddress, sizeof(publisherAddress)) == SOCKET_ERROR) {
        std::cerr << "Publisher bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(publisherListenSocket);
        WSACleanup();
        return 1;
    }

    // Bind subscriber socket
    if (bind(subscriberListenSocket, (sockaddr*)&subscriberAddress, sizeof(subscriberAddress)) == SOCKET_ERROR) {
        std::cerr << "Subscriber bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(subscriberListenSocket);
        WSACleanup();
        return 1;
    }

    //Listen kod publishera
    if (listen(publisherListenSocket, SOMAXCONN) == SOCKET_ERROR ||
        listen(subscriberListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(publisherListenSocket);
        closesocket(subscriberListenSocket);
        WSACleanup();
        return 1;
    }

    //Thread koji ce uvijek biti aktivan za slusanje kod subscribera
    HANDLE subscriberListenerThread = CreateThread(NULL, 0, SubscriberListenerThread, &subscriberListenSocket, 0, NULL);
    if (subscriberListenerThread == NULL) {
        std::cerr << "Failed to create subscriber listener thread.\n";
        closesocket(publisherListenSocket);
        closesocket(subscriberListenSocket);
        WSACleanup();
        return 1;
    }


    HANDLE heapMonitorThread = CreateThread(NULL, 0, MonitorHeapThread, &pq, 0, NULL);
    if (heapMonitorThread == NULL) {
        std::cerr << "Failed to create heap monitor thread.\n";
        closesocket(publisherListenSocket);
        closesocket(subscriberListenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening for publishers on port: " << DEFAULT_PORT << std::endl;
    std::cout << "Server is listening for subscribers on port: " << SUBSCRIBER_PORT << std::endl;
    fflush(stdout);
    SOCKET publisherSocket = accept(publisherListenSocket, NULL, NULL);
    if (publisherSocket != INVALID_SOCKET) {
        std::cout << "Publisher connected.\n";
        HandlePublisher(publisherSocket);
    }

    WaitForMultipleObjects(THREAD_POOL, consumerThreads, TRUE, INFINITE);


    // Cleanup resources
    for (int i = 0; i < THREAD_POOL; i++) {
        CloseHandle(consumerThreads[i]);
    }

    closesocket(publisherListenSocket);
    closesocket(subscriberListenSocket);
    DeleteCriticalSection(&cb.cs);
    CloseHandle(emptySemaphore);
    CloseHandle(fullSemaphore);
    CloseHandle(heapMonitorThread);
    CloseHandle(subscriberListenerThread);
    FreeHeap(&pq);
  //  CloseHandle(consumerThread);
    WSACleanup();

    return 0;
}