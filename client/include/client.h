#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
private:
    int sock;
    std::string trackerIP;
    int trackerPort;
    std::string recvBuf;

public:
    Client(std::string ip, int port);

    bool connectToTracker(bool quiet = false);
    void disconnect();

    bool isConnected() const;

    bool sendMessage(const std::string &msg);
    std::string receiveMessage();
};

#endif 
