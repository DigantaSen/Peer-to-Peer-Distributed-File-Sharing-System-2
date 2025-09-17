#include "../include/client.h"
#include "../include/commands.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>    // read, close
#include <fcntl.h>     // open
#include <cstring>     // strtok, strlen
#include <cstdlib>     // atoi

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <IP:PORT> <tracker_info.txt>\n";
        return 1;
    }

    // ---- Parse <IP:PORT> (this is the CLIENT’s listening address) ----
    std::string ipPort = argv[1];
    size_t colonPos = ipPort.find(':');
    if (colonPos == std::string::npos) {
        std::cerr << "Error: invalid format. Use <IP:PORT>\n";
        return 1;
    }
    std::string clientIP   = ipPort.substr(0, colonPos);
    int clientPort         = std::stoi(ipPort.substr(colonPos + 1));

    std::cout << "[CLIENT] My listening IP: " << clientIP
              << ", Port: " << clientPort << "\n";

    // ---- Read tracker_info.txt ----
    const char* infoFile = argv[2];
    int fd = open(infoFile, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    char buffer[1024];
    ssize_t n = read(fd, buffer, sizeof(buffer)-1);
    if (n < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    buffer[n] = '\0';
    close(fd);

    // ---- Parse trackers ----
    std::vector<std::pair<std::string,int>> trackers;
    char* line = strtok(buffer, "\n");
    while (line != nullptr) {
        char tIP[64];
        int tPort;
        if (sscanf(line, "%63s %d", tIP, &tPort) == 2) {
            trackers.push_back({std::string(tIP), tPort});
        }
        line = strtok(nullptr, "\n");
    }

    if (trackers.empty()) {
        std::cerr << "[CLIENT] No trackers found in " << infoFile << "\n";
        return 1;
    }

    // ---- Try to connect to whichever tracker is PRIMARY ----
    Client client("", 0);
    bool connected = false;

    for (auto &t : trackers) {
        std::cout << "[CLIENT] Trying tracker " << t.first << ":" << t.second << "\n";

        Client testClient(t.first, t.second);
        if (!testClient.connectToTracker()) {
            std::cerr << "[CLIENT] Connection failed to " << t.first << ":" << t.second << "\n";
            continue;
        }

        testClient.sendMessage("WHOAREYOU");
        std::string resp = testClient.receiveMessage();

        if (resp.find("ROLE PRIMARY") != std::string::npos) {
            std::cout << "[CLIENT] Connected to PRIMARY tracker at "
                    << t.first << ":" << t.second << "\n";

            client = std::move(testClient);  // ✅ move ownership
            connected = true;
            break;
        } else {
            std::cout << "[CLIENT] " << t.first << ":" << t.second
                    << " is BACKUP, disconnecting...\n";
            testClient.disconnect();
        }
    }

    if (!connected) {
        std::cerr << "[CLIENT] Could not connect to any PRIMARY tracker\n";
        return 1;
    }

    // ---- CLI loop ----
    while (true) {
        std::string input;
        std::cout << "client> ";
        std::getline(std::cin, input);

        if(input.empty()) continue;

        int result = handleCommand(client, input);
        if (result == 1) break;
        if (result == -1) continue;
        printf("kkkk\n");


        std::string response = client.receiveMessage();
        if (response.empty()) {
            std::cerr << "[CLIENT] Connection closed by tracker\n";
            break;
        }

        std::cout << "[TRACKER RESPONSE] " << response << "\n";
    }

    client.disconnect();
    return 0;
}
