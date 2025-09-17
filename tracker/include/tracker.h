#ifndef TRACKER_H
#define TRACKER_H

#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "SystemState.h"
#include "sync.h"

struct ClientSession {
    std::string userId;  // empty until login
    bool loggedIn = false;
};


class Tracker {
private:
    int myPort;
    std::string myIP;
    std::string peerIP;
    int peerPort;

    int server_fd;
    std::thread listenerThread;
    std::mutex stateMutex;
    std::atomic<bool> running{false};

    SystemState systemState;
    TrackerSync sync; 

    // Example state: user → password
    std::unordered_map<std::string, std::string> users;
    std::mutex sessionMutex;
    std::unordered_map<int, ClientSession> sessions;  // key = clientSock


      // default BACKUP

    void handleClient(int clientSock);
    void listenForClients();

public:
    Tracker(std::string ip, int port, std::string peerIP, int peerPort);
    ~Tracker();

    void start();
    void stop();

    void startSync();

    // Command handlers (simplified now, extend later)
    std::string processCommand(const std::string &cmd, int clientSock);

    // Role setters/getters

    enum Role { PRIMARY, BACKUP };
    Role role = BACKUP;

    void setRole(Role r);
    Role getRole() const;

    SystemState& getSystemState() { return systemState; }
};

#endif // TRACKER_H
