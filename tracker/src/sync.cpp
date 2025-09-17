#include "../include/sync.h"
#include "../include/SystemState.h"
#include "../include/tracker.h"
#include <iostream>
#include <unistd.h>
#include <sstream>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
using namespace std;



// Constructor
TrackerSync::TrackerSync(int mySyncPort, std::string peerIP, int peerSyncPort, 
                         Tracker &t, SystemState &s) : mySyncPort(mySyncPort),
      peerIP(peerIP),
      peerSyncPort(peerSyncPort),
      tracker(t),
      systemState(s) {
    listenSock = -1;
    connSock = -1;
}


// Decide role and connect to peer
void TrackerSync::start() {
    connectToPeer();
    if (connSock == -1) {
        tracker.setRole(Tracker::PRIMARY);
        cout << "[SYNC] Acting as PRIMARY\n";
        setupServerSocket();
        thread(&TrackerSync::acceptBackupConnection, this).detach();
    } else {
        // Peer is alive → become backup
        tracker.setRole(Tracker::BACKUP);
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
    peerAddr.sin_port = htons(peerSyncPort);
    inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);
    if (connect(connSock, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) < 0) {
        // Failed → assume peer not running
        printf("fffff\n");
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
    addr.sin_port = htons(mySyncPort);
    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenSock);
        exit(1);
    }
    if (listen(listenSock, 1) < 0) {
        perror("listen");
        close(listenSock);
        exit(1);
    }
    std::cout << "[SYNC] Primary listening on port " << mySyncPort << "\n";
}

void TrackerSync::acceptBackupConnection() {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    std::cout << "[SYNC] Waiting for backup to connect...\n";
    connSock = accept(listenSock, (struct sockaddr*)&clientAddr, &addrLen);
    if (connSock < 0) {
        perror("accept");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::thread(&TrackerSync::acceptBackupConnection, this).detach();
        return;
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
    std::cout << "[SYNC] Backup connected from " << ip << ":" << ntohs(clientAddr.sin_port) << "\n";
    std::thread(&TrackerSync::handlePeerUpdates, this).detach();
}

// Send update (only primary calls this)
void TrackerSync::sendUpdate(const std::string &update) {
    std::cout << "[DEBUG] sendUpdate called, role=" 
              << tracker.getRole()
              << " connSock=" << connSock
              << " update=" << update << "\n";
    if (tracker.getRole() != PRIMARY) return;
    
    {
        std::lock_guard<std::mutex> lock(logMutex);
        updateLog.push_back(update);  // save in log
    }
    if (connSock != -1) {
        printf("qqq %s %d lll", update.c_str(), tracker.getRole());
        sendMessage("UPDATE " + update);
    }
}

std::string TrackerSync::serializeSystemState() {
    std::string out;

    // Users
    for (auto &p : systemState.users) {
        const User &u = p.second;
        out += "USER " + u.userId + " " + u.passwordHash + " " +
               (u.loggedIn ? "1" : "0") + "\n";
    }

    // Groups
    for (auto &g : systemState.groups) {
        const Group &grp = g.second;
        out += "GROUP " + grp.groupId + " " + grp.ownerId + "\n";
        for (auto &m : grp.members) {
            out += "MEMBER " + grp.groupId + " " + m + "\n";
        }
        for (auto &r : grp.pendingRequests) {
            out += "REQUEST " + grp.groupId + " " + r + "\n";
        }
        for (auto &ord : grp.memberOrder) {
            out += "ORDER " + grp.groupId + " " + ord + "\n";
        }
    }

    // Files
    for (auto &g : systemState.groupFiles) {
        const std::string &gid = g.first;
        for (auto &f : g.second) {
            const FileMetadata &fm = f.second;
            out += "FILE " + gid + " " + fm.fileName + " " + fm.fileHash + "\n";
            for (auto &peer : fm.peers) {
                out += "PEER " + gid + " " + fm.fileName + " " + peer + "\n";
            }
        }
    }

    if (out.empty()) {
        out = "EMPTY\n";   // helpful for debugging
    }

    return out;
}


void TrackerSync::deserializeSystemState(const std::string &data) {
    systemState = SystemState(); // reset
    printf("Hiiii");
    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        std::istringstream ls(line);
        std::string type;
        ls >> type;

        if (type == "USER") {
            std::string userId, pass;
            std::string logged;
            ls >> userId >> pass >> logged;
            User u;
            u.userId = userId;
            u.passwordHash = pass;
            u.loggedIn = (logged == "1");
            systemState.users[userId] = u;
        } 
        else if (type == "GROUP") {
            std::string gid, owner;
            ls >> gid >> owner;
            Group g;
            g.groupId = gid;
            g.ownerId = owner;
            systemState.groups[gid] = g;
        } 
        else if (type == "MEMBER") {
            std::string gid, member;
            ls >> gid >> member;
            systemState.groups[gid].members.insert(member);
        } 
        else if (type == "REQUEST") {
            std::string gid, user;
            ls >> gid >> user;
            systemState.groups[gid].pendingRequests.insert(user);
        } 
        else if (type == "FILE") {
            std::string gid, fname, fhash;
            ls >> gid >> fname >> fhash;
            FileMetadata fm;
            fm.fileName = fname;
            fm.fileHash = fhash;
            systemState.groupFiles[gid][fname] = fm;
        } 
        else if (type == "PEER") {
            std::string gid, fname, peer;
            ls >> gid >> fname >> peer;
            systemState.groupFiles[gid][fname].peers.insert(peer);
        }
        else if (type == "ORDER") {
            std::string gid, user;
            ls >> gid >> user;
            systemState.groups[gid].memberOrder.push_back(user);
        }

    }
}

void TrackerSync::applyUpdate(const std::string &update) {
    char type[32], arg1[128], arg2[128], arg3[128];
    if (sscanf(update.c_str(), "%31s %127s %127s %127s", type, arg1, arg2, arg3) < 2) {
        return;
    }

    std::string t(type);

    if (t == "CREATE_USER") {
        User u;
        u.userId = arg1;
        u.passwordHash = arg2;
        systemState.users[u.userId] = u;
    } 
    else if (t == "GROUP_CREATE") {
        Group g;
        g.groupId = arg1;   // groupId
        g.ownerId = arg2;   // owner
        g.members.insert(arg2);
        g.memberOrder.push_back(arg2);  // owner is first
        systemState.groups[g.groupId] = g;
    }
    else if (t == "JOIN_REQUEST") {
        systemState.groups[arg1].pendingRequests.insert(arg2);
    }
    else if (t == "JOIN_ACCEPT") {
        systemState.groups[arg1].members.insert(arg2);
        systemState.groups[arg1].pendingRequests.erase(arg2);
        systemState.groups[arg1].memberOrder.push_back(arg2); // preserve join order
    }
    else if (t == "LEAVE_GROUP") {
        auto &grp = systemState.groups[arg1];
        std::string user = arg2;

        if (grp.ownerId == user) {
            // ownership transfer will already have been handled by primary’s state
            // just replicate
            grp.ownerId = ""; // backup will be corrected by full sync or explicit update
        }
        grp.members.erase(user);
        grp.memberOrder.erase(
            std::remove(grp.memberOrder.begin(), grp.memberOrder.end(), user),
            grp.memberOrder.end()
        );
    }
    else if (t == "UPLOAD_FILE") {
        FileMetadata fm;
        fm.fileName = arg2;
        fm.fileHash = arg3;
        systemState.groupFiles[arg1][fm.fileName] = fm;
    } 
    else if (t == "STOP_SHARE") {
        systemState.groupFiles[arg1].erase(arg2);
    }
}

// Receive updates from peer (for backup)
void TrackerSync::handlePeerUpdates() {
    std::cout << "[SYNC] handlePeerUpdates loop started\n";
    while (true) {
        std::string msg = recvMessage();
        std::cout << "[SYNC] Received raw: [" << msg << "]\n";

        if (msg == "__DISCONNECT__") {
            if (tracker.getRole() == PRIMARY) {
                std::cout << "[SYNC] Backup disconnected\n";
                if (connSock != -1) {
                    close(connSock);
                    connSock = -1;
                }
                std::thread(&TrackerSync::acceptBackupConnection, this).detach();
                return;
            } else {
                std::cout << "[SYNC] Peer disconnected, promoting to PRIMARY\n";
                promoteToPrimary();
                return;
            }
        }

        if (msg.empty()) {
            std::cout << "[SYNC] WARNING: empty message received\n";
            continue;
        }

        if (msg == "SYNC_REQUEST" && tracker.getRole() == PRIMARY) {
            std::cout << "[SYNC] Received SYNC_REQUEST from backup\n";
            std::string snapshot = serializeSystemState(); // may contain newlines
            // compress into one line, e.g. replace '\n' with ';'
            for (char &c : snapshot) {
                if (c == '\n') c = ';';
            }
            sendMessage("SYNC_RESPONSE " + snapshot);
            std::cout << "[SYNC] Sent full state to backup\n";
        } 
        else if (msg.rfind("SYNC_RESPONSE ", 0) == 0 && tracker.getRole() == BACKUP) {
            std::string stateStr = msg.substr(14);
            // convert ';' back to '\n'
            for (char &c : stateStr) {
                if (c == ';') c = '\n';
            }
            deserializeSystemState(stateStr);
            std::cout << "[SYNC] Loaded state from primary\n";
        } 
        else if (msg.rfind("UPDATE ", 0) == 0) {
            std::string update = msg.substr(7);
            applyUpdate(update);
            std::cout << "[SYNC] Applied update: " << update << "\n";
        } 
        else {
            std::cout << "[SYNC] Unknown message: " << msg << "\n";
        }
    }
}

// If backup, request sync from primary
void TrackerSync::syncFromPrimary() {
    // 🔹 Just send sync request, don’t read response here
    sendMessage("SYNC_REQUEST");
    std::cout << "[SYNC] Sent SYNC_REQUEST to Primary\n";
}

// Become primary if peer dies
void TrackerSync::promoteToPrimary() {
    if (tracker.getRole() == PRIMARY) return;  // already primary, don’t loop
    tracker.setRole(Tracker::PRIMARY);
    std::cout << "[SYNC] Promoted to PRIMARY (peer died)\n";
    if (connSock != -1) {
        printf("rrrr\n");
        close(connSock);
        connSock = -1;
    }

    tracker.start();
    // start listening for a new backup
    setupServerSocket();
    std::thread(&TrackerSync::acceptBackupConnection, this).detach();
}

// Replay log to new backup
void TrackerSync::replayLog() {
    if (tracker.getRole() != PRIMARY || connSock == -1) return;
    std::lock_guard<std::mutex> lock(logMutex);
    for (const auto &entry : updateLog) {
        sendMessage("UPDATE " + entry);
    }
    std::cout << "[SYNC] Replayed " << updateLog.size() << " updates to backup\n";
}

// Send message to peer
void TrackerSync::sendMessage(const std::string &msg) {
    if (connSock == -1) return;
    std::string data = msg;
    if (data.empty() || data.back() != '\n') {
        data.push_back('\n');
    }
    send(connSock, data.c_str(), data.size(), 0);
}


// Receive message (blocking)
std::string TrackerSync::recvMessage() {
    if (connSock == -1) return "__DISCONNECT__";

    std::string line;
    char ch;
    while (true) {
        int n = recv(connSock, &ch, 1, 0);
        if (n == 1) {
            if (ch == '\n') break;  // got full line
            line.push_back(ch);
        } else if (n == 0) {
            std::cout << "[SYNC] recvMessage(): peer closed connection\n";
            return "__DISCONNECT__";
        } else {
            if (errno == EINTR) continue;
            perror("[SYNC] recvMessage error");
            return "__DISCONNECT__";
        }
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    return line;
}
