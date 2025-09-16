#ifndef SYNC_H
#define SYNC_H

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
using namespace std;

enum TrackerRole { PRIMARY, BACKUP };

class TrackerSync {
private:
    TrackerRole role;
    int listenSock;
    int connSock;
    int myPort;
    string peerIP;
    int peerPort;

    mutex logMutex;
    vector<string> updateLog;

    std::atomic<bool> hasBackup{false};

    void sendMessage(const string &msg);
    string recvMessage();

public:
    TrackerSync(int myPort, string peerIP, int peerPort);

    void start();
    void connectToPeer();
    void sendUpdate(string &update);
    void handlePeerUpdates();
    void syncFromPrimary();
    void promoteToPrimary();
    void replayLog();

    void setupServerSocket();       // 🔹 NEW
    void acceptBackupConnection();

    TrackerRole getRole(){ 
        return role; 
    }


};

#endif