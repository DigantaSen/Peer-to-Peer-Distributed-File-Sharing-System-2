#include "../../common/include/sha1.h"
#include "../../common/include/protocol.h"
#include "../include/client.h"
#include <map>
#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <vector>
#include <string>
#include <sstream>
#include <openssl/sha.h>

using namespace std;

std::map<std::string, std::string> seedingFiles;

bool uploadFile(Client &client, const string &filePath, const string &groupId) {
    struct stat st;
    if (stat(filePath.c_str(), &st) < 0) {
        cerr << "[UPLOAD] Could not stat file: " << filePath
             << " (" << strerror(errno) << ")" << endl;
        return false;
    }
    size_t totalSize = st.st_size;

    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        cerr << "[UPLOAD] Could not open file: " << filePath
             << " (" << strerror(errno) << ")" << endl;
        return false;
    }

    const size_t CHUNK = 512 * 1024;
    vector<string> pieceHashes;

    char *buf = new char[CHUNK];
    ssize_t n;

    // Incremental SHA1 for the whole file
    SHA_CTX fullCtx;
    SHA1_Init(&fullCtx);

    while ((n = read(fd, buf, CHUNK)) > 0) {
        // per-piece SHA1
        char hashHex[41];
        computeSHA1(buf, n, hashHex);
        pieceHashes.push_back(string(hashHex));

        // update global SHA1
        SHA1_Update(&fullCtx, buf, n);
    }

    close(fd);
    delete[] buf;

    // Finalize full SHA1
    unsigned char digest[20];
    SHA1_Final(digest, &fullCtx);

    char fullHashHex[41];
    for (int i = 0; i < 20; i++) {
        sprintf(fullHashHex + (i * 2), "%02x", digest[i]);
    }
    fullHashHex[40] = '\0';
    string fullHash(fullHashHex);

    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

    // 🔑 Build metadata message with peer info
    ostringstream msg;
    msg << CMD_UPLOAD_FILE << " "
        << groupId << " "
        << client.getIP() << ":" << client.getPort() << " "
        << fileName << " "
        << totalSize << " "
        << pieceHashes.size() << " "
        << fullHash;

    for (const auto &h : pieceHashes) {
        msg << " " << h;
    }

    client.sendMessage(msg.str() + "\n");
    cout << "[UPLOAD] Metadata sent to tracker: "
         << fileName << " (" << totalSize << " bytes, "
         << pieceHashes.size() << " pieces, seeder="
         << client.getIP() << ":" << client.getPort() << ")" << endl;

    seedingFiles[fileName] = filePath;
    cout << "[DEBUG] Seeding " << fileName << " from path " << filePath << endl;

    return true;
}
