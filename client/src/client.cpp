#include "../include/client.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

Client::Client(std::string ip, int port) {
    trackerIP = ip;
    trackerPort = port;
    sock = -1;
}

bool Client::connectToTracker(bool quiet) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[CLIENT] Socket creation failed");
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(trackerPort);

    if (inet_pton(AF_INET, trackerIP.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("[CLIENT] Invalid address/Address not supported");
        close(sock);      // 🔹 prevent leak
        sock = -1;        // 🔹 mark invalid
        return false;
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[CLIENT] Connection failed");
        close(sock);
        sock = -1;
        return false;
    }

    if (!quiet) {
        std::cout << "[CLIENT] Connected to tracker at "
                  << trackerIP << ":" << trackerPort << "\n";
    }
    return true;
}


void Client::disconnect() {
    if (sock >= 0) {
        close(sock);
        sock = -1;
        std::cout << "[CLIENT] Disconnected from tracker\n";
    }
}

bool Client::isConnected() const {
    return sock >= 0;
}

bool Client::sendMessage(const std::string &msg) {
    if (sock < 0) return false;
    std::string data = msg + "\n";
    const char *ptr = data.c_str();
    size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t sent = send(sock, ptr, remaining, 0);
        if (sent < 0) {
            if (errno == EINTR) continue; // retry
            perror("[CLIENT] Send failed");
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }
    return true;
}


std::string Client::receiveMessage() {
    if (sock < 0) return "";

    char buffer[4096];
    ssize_t n = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) {
        return ""; // connection closed or error
    }

    buffer[n] = '\0';
    std::string line(buffer);

    // Trim CRLF
    if (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }

    return line;
}
