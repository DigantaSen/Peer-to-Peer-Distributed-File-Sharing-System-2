#ifndef SYNC_H
#define SYNC_H

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include "SystemState.h"
using namespace std;

// 🔹 Forward declaration to avoid circular include
class Tracker;

enum TrackerRole { PRIMARY, BACKUP };

class TrackerSync {
private:
    int listenSock;
    int connSock;

    int mySyncPort;        // ✅ my sync port (not client port)
    string peerIP;
    int peerSyncPort;      // ✅ peer’s sync port (not client port)

    mutex logMutex;
    vector<string> updateLog;

    Tracker &tracker;

    SystemState &systemState;

    std::atomic<bool> hasBackup{false};

    void sendMessage(const string &msg);
    string recvMessage();

    std::string serializeSystemState();
    void deserializeSystemState(const std::string &data);
    void applyUpdate(const std::string &update);

public:
    TrackerSync(int mySyncPort, std::string peerIP, int peerSyncPort, Tracker &t, SystemState &s);

    void start();
    void connectToPeer();
    void sendUpdate(const std::string &update);
    void handlePeerUpdates();
    void syncFromPrimary();
    void promoteToPrimary();
    void replayLog();

    void setupServerSocket();
    void acceptBackupConnection();

    
};

#endif
