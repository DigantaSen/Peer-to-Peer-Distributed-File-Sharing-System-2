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
    std::string recvBuffer;
    string userId;

    // NEW: client’s own listening address
    string selfIP;
    int selfPort;

public:
    // constructor now takes both
    Client(string ip, int port);
    // ~Client();

    bool connectToTracker(bool quiet = false);
    void disconnect();

    bool isConnected() const;

    bool sendMessage(const string &msg);
    string receiveMessage();

    // NEW: setters for client’s own address
    void setSelfAddress(const string &ip, int port) {
        selfIP = ip;
        selfPort = port;
    }

    // NEW: getters for client’s own address
    string getIP() const { return selfIP; }
    int getPort() const { return selfPort; }

    void setUserId(const string &id) { userId = id; }
    string getUserId() const { return userId; }
};

#endif
