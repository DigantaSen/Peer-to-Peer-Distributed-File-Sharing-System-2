#include "../include/tracker.h"
#include "../../common/include/protocol.h"  
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

Tracker::Tracker(std::string ip, int port, std::string peerIP, int peerPort)
    : myIP(ip),
      myPort(port),
      peerIP(peerIP),
      peerPort(peerPort),
      server_fd(-1),
      role(BACKUP),
      sync(port + 100, peerIP, peerPort + 100, *this, systemState)  // pass state too
{}

void Tracker::startSync() {
    // call start() on the internal TrackerSync member so only one sync exists
    sync.start();
}

Tracker::~Tracker() {
    stop();
}

void Tracker::setRole(Role r) {
    role = r;
    std::cout << "[TRACKER] Role set to " << (role == PRIMARY ? "PRIMARY" : "BACKUP") << "\n";
}

Tracker::Role Tracker::getRole() const { return role; }

void Tracker::start() {
    if (running.load()) return;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[TRACKER] Socket creation failed");
        server_fd = -1;
        return;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[TRACKER] setsockopt failed");
        close(server_fd);
        server_fd = -1;
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(myPort);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[TRACKER] Bind failed");
        close(server_fd);
        server_fd = -1;
        return;
    }

    if (listen(server_fd, 10) < 0) {
        perror("[TRACKER] Listen failed");
        close(server_fd);
        server_fd = -1;
        return;
    }

    std::cout << "[TRACKER] Listening for clients on port " << myPort << "\n";

    running.store(true);
    listenerThread = std::thread(&Tracker::listenForClients, this);
}

void Tracker::stop() {
    running.store(false);                     // 🔹 NEW

    if (server_fd >= 0) {
        shutdown(server_fd, SHUT_RDWR);       // 🔹 NEW: unblock accept()
        close(server_fd);
        server_fd = -1;
    }

    if (listenerThread.joinable()) {          // 🔹 NEW
        listenerThread.join();                // 🔹 NEW: wait for clean exit
    }
}

void Tracker::listenForClients() {
    while (running.load()) {                  // 🔹 CHANGE: stop loop when shutting down
        int clientSock = accept(server_fd, nullptr, nullptr);
        if (clientSock < 0) {
            if (!running.load()) break;       // 🔹 NEW: exit cleanly on stop()
            if (errno == EINTR) continue;     // 🔹 NEW: retry on EINTR
            perror("[TRACKER] Accept failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 🔹 NEW: avoid spin
            continue;
        }

        try {
            std::thread(&Tracker::handleClient, this, clientSock).detach();
        } catch (...) {
            close(clientSock);                // 🔹 NEW: avoid leak if thread creation fails
        }
    }
}

void Tracker::handleClient(int clientSock) {
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        sessions[clientSock] = ClientSession();   // 🔹 NEW
    }
    char buffer[4096];
    while (running.load()) {                  // 🔹 CHANGE: exit if shutting down
        ssize_t bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            std::string msg(buffer, static_cast<size_t>(bytes));
            

            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) {
                msg.pop_back();
            }
            std::cout << "[TRACKER] Received raw: [" << msg << "]\n";

            std::string response;
            if (msg == "WHOAREYOU") {
                response = (role == PRIMARY ? "ROLE PRIMARY" : "ROLE BACKUP");
            } else {
                response = processCommand(msg, clientSock);
                std::cout<<"Hiiiii"<<role<<"pppp\n";
            }

            response += "\n";

            const char *ptr = response.c_str();        // 🔹 NEW robust send loop
            size_t toSend = response.size();
            while (toSend > 0) {
                ssize_t sent = send(clientSock, ptr, toSend, 0);
                if (sent < 0) {
                    if (errno == EINTR) continue;      // 🔹 retry on EINTR
                    perror("[TRACKER] Send failed");
                    close(clientSock);                 // 🔹 ensure no leak
                    return;
                }
                ptr += sent;
                toSend -= static_cast<size_t>(sent);
            }
        }
        else if (bytes == 0) {
            std::cout << "[TRACKER] Client disconnected\n";
            break;
        }
        else {
            if (errno == EINTR) continue;              // 🔹 retry
            perror("[TRACKER] Recv failed");
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        sessions.erase(clientSock);   // 🔹 NEW
    }
    close(clientSock);                                 // 🔹 already present, keep
}

std::string Tracker::processCommand(const std::string &cmd, int clientSock) {
    // Split command into tokens (basic parsing)
    std::istringstream iss(cmd);
    std::string action;
    iss >> action;

    if (action == CMD_CREATE_USER) {
        std::string user, pass;
        iss >> user >> pass;
        if (user.empty() || pass.empty()) {
            return std::string(PROTO_ERR) + " " + std::to_string(ERR_INTERNAL) + " Missing arguments";
        }

        std::string errMsg;
        if (!systemState.createUser(user, pass, errMsg)) {
            return std::string(PROTO_ERR) + " " + std::to_string(ERR_INTERNAL) + " " + errMsg;
        }

        sync.sendUpdate(std::string("CREATE_USER ") + user + " " + pass);
        printf("kjjs");

        return std::string(PROTO_OK) + " User created: " + user;
    }
    else if (action == CMD_LOGIN) {
        std::string user, pass;
        iss >> user >> pass;
        if (user.empty() || pass.empty()) {
            return std::string(PROTO_ERR) + " Missing arguments";
        }

        std::string errMsg;
        if (!systemState.loginUser(user, pass, errMsg)) {
            return std::string(PROTO_ERR) + " " + errMsg;
        }

        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            sessions[clientSock].userId = user;
            sessions[clientSock].loggedIn = true;
        }

        return std::string(PROTO_OK) + " Logged in as " + user;
    }
    else if (action == CMD_CREATE_GROUP) {
        std::string gid;
        iss >> gid;
        if (gid.empty()) {
            return std::string(PROTO_ERR) + " Missing group_id";
        }

        std::string owner;
        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            if (!sessions[clientSock].loggedIn) {
                return std::string(PROTO_ERR) + " Must be logged in";
            }
            owner = sessions[clientSock].userId;
        }

        std::string errMsg;
        if (!systemState.createGroup(gid, owner, errMsg)) {
            return std::string(PROTO_ERR) + " " + errMsg;
        }

        sync.sendUpdate("GROUP_CREATE " + gid + " " + owner);

        return std::string(PROTO_OK) + " Group created: " + gid;
    }
    else if (action == "list_groups") {
        std::string user;
        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            if (!sessions[clientSock].loggedIn) {
                return std::string(PROTO_ERR) + " Must be logged in";
            }
            user = sessions[clientSock].userId;
        }

        auto allGroups = systemState.listGroups();
        if (allGroups.empty()) {
            return std::string(PROTO_OK) + " No groups available";
        }

        std::string result = std::string(PROTO_OK) + " Groups:\n";
        for (auto &g : allGroups) {
            result += " - " + g + "\n";
        }

        return result;
    }

    else {
        // Unknown command
        return std::string(PROTO_ERR) + " " + std::to_string(ERR_INTERNAL) + " Unknown command: " + action;
    }
}
