// uploader_server.cpp
#include "../include/uploader.h"
#include "../include/uploader_server.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sstream>

using namespace std;

std::mutex seedingFilesMutex; // protects seedingFiles declared extern in uploader.h

// Helper: send text and close
static void sendErrAndClose(int sock, const string &msg) {
    send(sock, msg.c_str(), msg.size(), 0);
    close(sock);
}

static void handlePeerRequest(int clientSock) {
    // read request (small), single recv is fine for this command syntax
    char buffer[1024];
    ssize_t n = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(clientSock);
        return;
    }
    buffer[n] = '\0';

    // parse
    string cmd;
    string fileName;
    long long pieceIndex = -1;
    {
        istringstream iss(buffer);
        iss >> cmd >> fileName >> pieceIndex;
    }

    if (cmd != "GET_PIECE" || fileName.empty() || pieceIndex < 0) {
        cerr << "[SEEDER] Invalid request: " << buffer << endl;
        sendErrAndClose(clientSock, string("ERR Invalid request\n"));
        return;
    }

    // lookup file path
    string filePath;
    {
        lock_guard<mutex> lock(seedingFilesMutex);
        auto it = seedingFiles.find(fileName);
        if (it == seedingFiles.end()) {
            cerr << "[SEEDER] Not seeding file: " << fileName << endl;
            sendErrAndClose(clientSock, string("ERR File not available\n"));
            return;
        }
        filePath = it->second;
    }

    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("[SEEDER] open");
        sendErrAndClose(clientSock, string("ERR Could not open file\n"));
        return;
    }

    // get file size
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("[SEEDER] fstat");
        close(fd);
        sendErrAndClose(clientSock, string("ERR Could not stat file\n"));
        return;
    }
    off_t totalSize = st.st_size;

    const size_t CHUNK = 512 * 1024; // must match client
    off_t offset = pieceIndex * (off_t)CHUNK;

    if (offset < 0 || offset >= totalSize) {
        cerr << "[SEEDER] Invalid piece index " << pieceIndex << " for " << fileName << endl;
        close(fd);
        sendErrAndClose(clientSock, string("ERR Invalid piece index\n"));
        return;
    }

    size_t expectedSize = CHUNK;
    if (offset + (off_t)CHUNK > totalSize) {
        expectedSize = (size_t)(totalSize - offset);
    }

    // read exactly expectedSize using pread (thread-safe w.r.t file offset)
    vector<char> buf(expectedSize);
    ssize_t r = pread(fd, buf.data(), expectedSize, offset);
    close(fd);

    if (r < 0) {
        perror("[SEEDER] pread");
        sendErrAndClose(clientSock, string("ERR Read failed\n"));
        return;
    }
    if ((size_t)r != expectedSize) {
        cerr << "[SEEDER] short read: expected " << expectedSize << " got " << r << endl;
        // still send what we have (but better to signal error)
        sendErrAndClose(clientSock, string("ERR Short read\n"));
        return;
    }

    // send the data fully
    size_t sent = 0;
    while (sent < expectedSize) {
        ssize_t s = send(clientSock, buf.data() + sent, expectedSize - sent, 0);
        if (s < 0) {
            if (errno == EINTR) continue;
            perror("[SEEDER] send");
            break;
        }
        sent += (size_t)s;
    }

    cout << "[SEEDER] Sent piece " << pieceIndex << " of " << fileName << " (" << expectedSize << " bytes)\n";
    close(clientSock);
}

bool startUploaderServer(const string &ip, int port) {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("[SEEDER] socket");
        return false;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        cerr << "[SEEDER] Invalid IP: " << ip << endl;
        close(serverSock);
        return false;
    }

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[SEEDER] bind");
        close(serverSock);
        return false;
    }

    if (listen(serverSock, 16) < 0) {
        perror("[SEEDER] listen");
        close(serverSock);
        return false;
    }

    cout << "[SEEDER] Listening for peers on " << ip << ":" << port << endl;

    thread([serverSock]() {
        while (true) {
            int clientSock = accept(serverSock, nullptr, nullptr);
            if (clientSock < 0) {
                perror("[SEEDER] accept");
                continue;
            }
            try {
                thread(handlePeerRequest, clientSock).detach();
            } catch (...) {
                close(clientSock);
            }
        }
    }).detach();

    return true;
}
