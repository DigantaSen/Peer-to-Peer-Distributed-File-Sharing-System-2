#include "../include/tracker.h"
#include "../include/sync.h"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>    // sleep
#include <fcntl.h>     // open
#include <cstring>     // strtok, strlen
#include <cstdlib>     // atoi

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <tracker_info.txt> <tracker_no>\n";
        return 1;
    }

    const char* infoFile = argv[1];
    int trackerNo = std::stoi(argv[2]);

    // ---- Read file into buffer using open/read ----
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
    buffer[n] = '\0';  // null terminate
    close(fd);

    // ---- Parse lines ----
    std::vector<std::pair<std::string,int>> trackers;
    char* line = strtok(buffer, "\n");
    while (line != nullptr) {
        char ip[64];
        int port;
        if (sscanf(line, "%63s %d", ip, &port) == 2) {
            trackers.push_back({std::string(ip), port});
        }
        line = strtok(nullptr, "\n");
    }

    if (trackerNo < 1 || trackerNo > (int)trackers.size()) {
        std::cerr << "Error: invalid tracker_no\n";
        return 1;
    }

    // My info
    std::string myIP   = trackers[trackerNo-1].first;
    int myPort         = trackers[trackerNo-1].second;

    // Peer info (only 2 trackers expected)
    int peerNo = (trackerNo == 1 ? 2 : 1);
    std::string peerIP = trackers[peerNo-1].first;
    int peerPort       = trackers[peerNo-1].second;

    std::cout << "[MAIN] Tracker " << trackerNo 
              << " running at " << myIP << ":" << myPort
              << ", peer at " << peerIP << ":" << peerPort << "\n";

    int syncPort      = myPort + 1000;     // my sync port
    int peerSyncPort  = peerPort + 1000;   // peer sync port

    // ---- Start client listener ----
    Tracker tracker(myIP, myPort, peerIP, peerPort);

// ---- Start sync with peer tracker ----

    // TrackerSync sync(syncPort, peerIP, peerSyncPort, tracker, tracker.getSystemState());
    // sync.start();

    tracker.startSync();

    if (tracker.getRole() == Tracker::PRIMARY) {
        tracker.start();
    }

    // Keep alive
    while (true) {
        sleep(10);
    }

    return 0;
}
