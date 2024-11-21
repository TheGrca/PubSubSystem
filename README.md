SmartGrid PubSub System is a scalable and efficient Publish-Subscribe (PubSub) service designed for real-time monitoring and distribution of power grid data.
The system enables publishers to share data related to current strength, voltage, and power across various city locations,
while subscribers can register to receive updates for specific topics or locations.

Features:
TCP-based Communication: Reliable data transmission between publishers, subscribers, and the server.
Subscription Management: Subscribers can register for specific topics (e.g., current strength, voltage, power) and city locations (0-999).
Historical Data Retrieval: Subscribers joining late can retrieve historical data stored in a heap.
Efficient Data Storage: The server uses hashmaps to manage topics and locations, each pointing to lists of subscribers.
Thread Management: Threads handle data publishing and delivery to subscribers in parallel.
Data Expiration: Old data is automatically removed after its expiration time.
Stress Testing: The system is tested with a large dataset (~10,000 messages) to ensure scalability and performance.

This project showcases an optimal implementation of a PubSub system tailored for smart grid data distribution and analysis, leveraging custom data structures and threading for high performance.