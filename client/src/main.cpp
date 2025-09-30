#include "../include/client.h"
#include "../include/commands.h"
#include "../include/downloader.h"
#include "../include/uploader_server.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
using namespace std;

std::string lastDestPath;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr<<"Usage: "<<argv[0]<<" <IP:PORT> <tracker_info.txt>\n";
        return 1;
    }

    string ipPort = argv[1];
    size_t colonPos = ipPort.find(':');
    if (colonPos == string::npos) {
        cerr<<"Error: invalid format. Use <IP:PORT>\n";
        return 1;
    }
    string clientIP = ipPort.substr(0, colonPos);
    int clientPort = stoi(ipPort.substr(colonPos + 1));

    cout<<"[CLIENT] My listening IP: "<<clientIP<<", Port: "<<clientPort<<"\n";

    if (!startUploaderServer(clientIP, clientPort)) {
        cerr << "[CLIENT] Failed to start uploader server\n";
        return 1;
    }

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

    vector<pair<string,int>> trackers;
    char* line = strtok(buffer, "\n");
    while (line != nullptr) {
        char tIP[64];
        int tPort;
        if (sscanf(line, "%63s %d", tIP, &tPort) == 2) {
            trackers.push_back({string(tIP), tPort});
        }
        line = strtok(nullptr, "\n");
    }

    if (trackers.empty()) {
        cerr<<"[CLIENT] No trackers found in "<<infoFile<<"\n";
        return 1;
    }

    Client client("", 0);
    bool connected = false;

    for (auto &t : trackers) {
        cout<<"[CLIENT] Trying tracker "<<t.first<<":"<<t.second<<"\n";

        Client testClient(t.first, t.second);
        if (!testClient.connectToTracker()) {
            cerr<<"[CLIENT] Connection failed to "<<t.first<<":"<<t.second<<"\n";
            continue;
        }

        testClient.sendMessage("WHOAREYOU");
        string resp = testClient.receiveMessage();

        if (resp.find("ROLE PRIMARY") != string::npos) {
            cout<<"[CLIENT] Connected to PRIMARY tracker at "<<t.first<<":"<<t.second<<"\n";

            client = move(testClient);
            connected = true;
            break;
        } else {
            cout<<"[CLIENT] "<<t.first<<":"<< t.second<<" is BACKUP, disconnecting...\n";
            testClient.disconnect();
        }
    }

    if (!connected) {
        cerr<<"[CLIENT] Could not connect to any PRIMARY tracker\n";
        return 1;
    }

    client.setSelfAddress(clientIP, clientPort);

    while (true) {
        string input;
        cout<<"client> ";
        getline(cin, input);

        if(input.empty()) continue;

        int result = handleCommand(client, input);
        if (result == 1) break;
        if (result == -1) continue;
        // printf("kkkk\n");


        string response = client.receiveMessage();
        if (response.empty()) {
            cerr<<"[CLIENT] Connection closed by tracker\n";
            break;
        }

        if (response.rfind("OK DOWNLOAD", 0) == 0) {
            // Special case: download metadata
            DownloadMetadata meta = parseDownloadResponse(response);
            if (!startDownload(client, meta, lastDestPath)) {
                cerr << "[CLIENT] Download failed\n";
            }
        } else {
            cout<<"[TRACKER RESPONSE] "<<response<<"\n";
        }
    }

    client.disconnect();
    return 0;
}
