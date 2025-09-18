#ifndef CLIENT_H
#define CLIENT_H

#include <string>
using namespace std;

class Client {
private:
    int sock;
    string trackerIP;
    int trackerPort;
    string recvBuf;

public:
    Client(string ip, int port);

    bool connectToTracker(bool quiet = false);
    void disconnect();

    bool isConnected() const;

    bool sendMessage(const string &msg);
    string receiveMessage();
};

#endif 
