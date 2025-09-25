#include "../include/uploader.h"
#include "../../common/include/sha1.h"
#include "../../common/include/protocol.h"
#include "../include/client.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sstream>
using namespace std;

bool uploadFile(Client &client, const string &filePath, const string &groupId) {
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        cerr << "[UPLOAD] Could not open file: " << filePath << endl;
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        cerr << "[UPLOAD] Could not stat file: " << filePath << endl;
        close(fd);
        return false;
    }
    size_t totalSize = st.st_size;

    const size_t CHUNK = 512 * 1024; // 512 KB
    vector<string> pieceHashes;

    char *buf = new char[CHUNK];
    ssize_t n;

    string fullContent;
    fullContent.reserve(totalSize);

    while ((n = read(fd, buf, CHUNK)) > 0) {
        // Compute SHA1 for this piece
        char hashHex[41];
        computeSHA1(buf, n, hashHex);
        pieceHashes.push_back(string(hashHex));

        // Accumulate for full file SHA1
        fullContent.append(buf, n);
    }
    close(fd);
    delete[] buf;

    // Compute full file SHA1
    char fullHashHex[41];
    computeSHA1(fullContent.data(), fullContent.size(), fullHashHex);
    string fullHash(fullHashHex);

    string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

    // Build metadata message
    ostringstream msg;
    msg << CMD_UPLOAD_FILE << " "
        << groupId << " "
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
         << pieceHashes.size() << " pieces)" << endl;

    return true;
}