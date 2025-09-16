#include "../include/sync.h"
#include "../include/state.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
using namespace std;

static SystemState state;

// Constructor
TrackerSync::TrackerSync(int myPort, std::string peerIP, int peerPort){
    this->myPort = myPort;
    this->peerIP = peerIP;
    this->peerPort = peerPort;
    role = BACKUP;  // default, corrected later in start()
    listenSock = -1;
    connSock = -1;
}

// Decide role and connect to peer
void TrackerSync::start() {
    connectToPeer();
    if (connSock == -1) {
        role = PRIMARY;
        cout << "[SYNC] Acting as PRIMARY\n";
        setupServerSocket();
        thread(&TrackerSync::acceptBackupConnection, this).detach();
    } else {
        // Peer is alive → become backup
        role = BACKUP;
        std::cout << "[SYNC] Acting as BACKUP\n";
        syncFromPrimary();
        std::thread(&TrackerSync::handlePeerUpdates, this).detach();
    }
}

// Try to connect to peer
void TrackerSync::connectToPeer() {
    connSock = socket(AF_INET, SOCK_STREAM, 0);
    if (connSock < 0) {
        perror("socket");
        connSock = -1;
        return;
    }
    struct sockaddr_in peerAddr;
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peerPort);
    inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);
    if (connect(connSock, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) < 0) {
        // Failed → assume peer not running
        close(connSock);
        connSock = -1;
    }
}

void TrackerSync::setupServerSocket() {
    listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock < 0) {
        perror("socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // listen on all interfaces
    addr.sin_port = htons(myPort);
    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    if (listen(listenSock, 1) < 0) {
        perror("listen");
        exit(1);
    }
    std::cout << "[SYNC] Primary listening on port " << myPort << "\n";
}

void TrackerSync::acceptBackupConnection() {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    std::cout << "[SYNC] Waiting for backup to connect...\n";
    connSock = accept(listenSock, (struct sockaddr*)&clientAddr, &addrLen);
    if (connSock < 0) {
        perror("accept");
        return;
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
    std::cout << "[SYNC] Backup connected from " << ip << ":" << ntohs(clientAddr.sin_port) << "\n";
    std::thread(&TrackerSync::handlePeerUpdates, this).detach();
}

// Send update (only primary calls this)
void TrackerSync::sendUpdate(string &update) {
    if (role != PRIMARY) return;
    {
        std::lock_guard<std::mutex> lock(logMutex);
        updateLog.push_back(update);  // save in log
    }
    if (connSock != -1) {
        sendMessage("UPDATE " + update);
    }
}

std::string serializeSystemState() {
    std::string out;
    // Users
    for (auto &p : state.users) {
        const User &u = p.second;
        out += "USER " + u.userId + " " + u.passwordHash + " " + (u.loggedIn ? "1" : "0") + "\n";
    }
    // Groups
    for (auto &g : state.groups) {
        const Group &grp = g.second;
        out += "GROUP " + grp.groupId + " " + grp.ownerId + "\n";
        for (auto &m : grp.members) {
            out += "MEMBER " + grp.groupId + " " + m + "\n";
        }
        for (auto &r : grp.pendingRequests) {
            out += "REQUEST " + grp.groupId + " " + r + "\n";
        }
    }
    // Files
    for (auto &g : state.groupFiles) {
        std::string gid = g.first;
        for (auto &f : g.second) {
            const FileMetadata &fm = f.second;
            out += "FILE " + gid + " " + fm.fileName + " " + fm.fileHash + "\n";
            for (auto &peer : fm.peers) {
                out += "PEER " + gid + " " + fm.fileName + " " + peer + "\n";
            }
        }
    }
    if (out.empty()) {
        out = "EMPTY\n";   // ✅ helps verify with telnet
    }
    return out;
}

void deserializeSystemState(const std::string &data) {
    state = SystemState(); // reset
    char buf[1024];
    strncpy(buf, data.c_str(), sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    char *line = strtok(buf, "\n");
    while (line) {
        std::string type;
        char arg1[128], arg2[128], arg3[128];
        if (sscanf(line, "%127s %127s %127s %127s", arg1, arg2, arg3, (char*)&arg3+1) >= 2) {
            type = arg1;
            if (type == "USER") {
                User u;
                u.userId = arg2;
                u.passwordHash = arg3;
                u.loggedIn = (strstr(line, " 1") != nullptr);
                state.users[u.userId] = u;
            } else if (type == "GROUP") {
                Group g;
                g.groupId = arg2;
                g.ownerId = arg3;
                state.groups[g.groupId] = g;
            } else if (type == "MEMBER") {
                state.groups[arg2].members.insert(arg3);
            } else if (type == "REQUEST") {
                state.groups[arg2].pendingRequests.insert(arg3);
            } else if (type == "FILE") {
                FileMetadata fm;
                fm.fileName = arg3;
                fm.fileHash = std::string(strchr(line, ' ') + 1); // crude parse
                state.groupFiles[arg2][fm.fileName] = fm;
            } else if (type == "PEER") {
                state.groupFiles[arg2][arg3].peers.insert(std::string(strrchr(line, ' ') + 1));
            }
        }
        line = strtok(nullptr, "\n");
    }
}

void applyUpdate(const std::string &update) {
    char type[32], arg1[128], arg2[128], arg3[128];
    if (sscanf(update.c_str(), "%31s %127s %127s %127s", type, arg1, arg2, arg3) < 2) return;
    std::string t(type);
    if (t == "USER_CREATE") {
        User u;
        u.userId = arg1;
        u.passwordHash = arg2;
        state.users[u.userId] = u;
    } else if (t == "GROUP_CREATE") {
        Group g;
        g.groupId = arg1;
        g.ownerId = arg2;
        state.groups[g.groupId] = g;
    } else if (t == "JOIN_REQUEST") {
        state.groups[arg1].pendingRequests.insert(arg2);
    } else if (t == "JOIN_ACCEPT") {
        state.groups[arg1].members.insert(arg2);
        state.groups[arg1].pendingRequests.erase(arg2);
    } else if (t == "UPLOAD_FILE") {
        FileMetadata fm;
        fm.fileName = arg2;
        fm.fileHash = arg3;
        state.groupFiles[arg1][fm.fileName] = fm;
    } else if (t == "STOP_SHARE") {
        state.groupFiles[arg1].erase(arg2);
    }
}

// Receive updates from peer (for backup)
void TrackerSync::handlePeerUpdates() {
    while (true) {
        std::string msg = recvMessage();
        if (msg.empty()) {
            if (role == PRIMARY) {
                std::cout << "[SYNC] Backup disconnected\n";
                if (connSock != -1) {
                    close(connSock);
                    connSock = -1;
                }
                std::thread(&TrackerSync::acceptBackupConnection, this).detach();
                return;
            } else if (role == BACKUP) {
                std::cout << "[SYNC] Peer disconnected\n";
                promoteToPrimary();
                return;
            }
            
        }
        if (msg == "SYNC_REQUEST" && role == PRIMARY) {
            std::string snapshot = serializeSystemState();
            sendMessage("SYNC_RESPONSE " + snapshot);
            std::cout << "[SYNC] Sent full state to backup\n";
        } else if (msg.rfind("SYNC_RESPONSE ", 0) == 0 && role == BACKUP) {
            std::string stateStr = msg.substr(14);
            deserializeSystemState(stateStr);
            std::cout << "[SYNC] Loaded state from primary\n";
        } else if (msg.rfind("UPDATE ", 0) == 0) {
            std::string update = msg.substr(7);
            applyUpdate(update);
            std::cout << "[SYNC] Applied update: " << update << "\n";
        }
    }
}

// If backup, request sync from primary
void TrackerSync::syncFromPrimary() {
    sendMessage("SYNC_REQUEST");
    std::string msg = recvMessage();
    if (msg.empty()) {
        std::cout << "[SYNC] Primary died during sync\n";
        promoteToPrimary();
        return;
    }
    if (msg.rfind("SYNC_RESPONSE ", 0) == 0) {
        std::string serialized = msg.substr(14);
        std::cout << "[SYNC] Received state snapshot from Primary\n";
        deserializeSystemState(serialized);
    } else {
        std::cerr << "[SYNC] Unexpected response from Primary: " << msg << "\n";
    }
}

// Become primary if peer dies
void TrackerSync::promoteToPrimary() {
    if (role == PRIMARY) return;  // already primary, don’t loop
    role = PRIMARY;
    std::cout << "[SYNC] Promoted to PRIMARY (peer died)\n";
    if (connSock != -1) {
        close(connSock);
        connSock = -1;
    }
    // start listening for a new backup
    setupServerSocket();
    std::thread(&TrackerSync::acceptBackupConnection, this).detach();
}

// Replay log to new backup
void TrackerSync::replayLog() {
    if (role != PRIMARY || connSock == -1) return;
    std::lock_guard<std::mutex> lock(logMutex);
    for (const auto &entry : updateLog) {
        sendMessage("UPDATE " + entry);
    }
    std::cout << "[SYNC] Replayed " << updateLog.size() << " updates to backup\n";
}

// Send message to peer
void TrackerSync::sendMessage(const std::string &msg) {
    if (connSock == -1) return;
    std::string data = msg + "\n";
    send(connSock, data.c_str(), data.size(), 0);
}

// Receive message (blocking)
std::string TrackerSync::recvMessage() { 
    char buffer[1024]; 
    int n = recv(connSock, buffer, sizeof(buffer)-1, 0); 
    if (n <= 0) return ""; 

    buffer[n] = '\0';
    return std::string(buffer); 
}