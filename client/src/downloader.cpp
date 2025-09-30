// downloader.cpp  (replace your existing downloader.cpp with this)
#include "../include/downloader.h"
#include "../../common/include/sha1.h"
#include <openssl/sha.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <random>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>
#include <map>
#include <sys/time.h>

using namespace std;

static const size_t CHUNK = 512 * 1024; // 512 KiB

static bool verifyFullFile(const DownloadMetadata &meta, const std::string &destPath) {
    int fd = open(destPath.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("[CLIENT] open for verify");
        return false;
    }

    const size_t BUF = 512 * 1024;
    std::vector<char> buf(BUF);
    ssize_t n;

    // Use OpenSSL streaming SHA1 to avoid buffering entire file
    SHA_CTX ctx;
    if (SHA1_Init(&ctx) != 1) {
        cerr << "[CLIENT] SHA1_Init failed\n";
        close(fd);
        return false;
    }

    while ((n = read(fd, buf.data(), BUF)) > 0) {
        if (SHA1_Update(&ctx, reinterpret_cast<unsigned char*>(buf.data()), (size_t)n) != 1) {
            cerr << "[CLIENT] SHA1_Update failed\n";
            close(fd);
            return false;
        }
    }
    if (n < 0) {
        perror("[CLIENT] read during verify");
        close(fd);
        return false;
    }

    unsigned char digest[SHA_DIGEST_LENGTH];
    if (SHA1_Final(digest, &ctx) != 1) {
        cerr << "[CLIENT] SHA1_Final failed\n";
        close(fd);
        return false;
    }

    close(fd);

    // Convert digest -> hex string
    char hashHex[41];
    for (int i = 0; i < (int)SHA_DIGEST_LENGTH; ++i) {
        sprintf(hashHex + (i * 2), "%02x", digest[i]);
    }
    hashHex[40] = '\0';

    if (meta.fullHash != std::string(hashHex)) {
        cerr << "[CLIENT] Final hash mismatch! Expected "
             << meta.fullHash << " but got " << hashHex << endl;
        return false;
    }

    cout << "[CLIENT] Final file hash verified successfully!" << endl;
    return true;
}

// ...existing code...
DownloadMetadata parseDownloadResponse(const string &response) {
    cout << "[DEBUG] Full response length: " << response.length() << endl;
    cout << "[DEBUG] Response: " << response.substr(0, min(response.length(), (size_t)200)) << "..." << endl;
    
    DownloadMetadata meta;
    istringstream iss(response);
    string status, keyword;
    iss >> status >> keyword;
    
    cout << "[DEBUG] Status: '" << status << "', Keyword: '" << keyword << "'" << endl;
    
    if (status != "OK" || keyword != "DOWNLOAD") {
        cerr << "[CLIENT] Invalid download response status/keyword" << endl;
        return meta;
    }

    iss >> meta.fileName >> meta.fileSize >> meta.numChunks >> meta.fullHash;
    
    cout << "[DEBUG] Parsed: file=" << meta.fileName 
         << " size=" << meta.fileSize 
         << " chunks=" << meta.numChunks 
         << " hash=" << meta.fullHash << endl;

    // Read exactly numChunks piece hashes
    meta.pieceHashes.clear();
    meta.pieceHashes.reserve(meta.numChunks);
    for (size_t i = 0; i < meta.numChunks; ++i) {
        string h;
        if (!(iss >> h)) {
            cerr << "[CLIENT] Incomplete piece hash list (got " << meta.pieceHashes.size()
                 << " of " << meta.numChunks << ")\n";
            return DownloadMetadata{};
        }
        meta.pieceHashes.push_back(h);
    }

    // Then parse peers as "PEER <ip:port>" pairs
    meta.peers.clear();
    string tok;
    while (iss >> tok) {
        if (tok == "PEER") {
            string peer;
            if (iss >> peer) meta.peers.push_back(peer);
        }
    }

    if (meta.peers.empty()) {
        cerr << "[CLIENT] No peers provided for download\n";
    }

    cout << "[DEBUG] Final parsed pieces: " << meta.pieceHashes.size() 
         << ", peers: " << meta.peers.size() << endl;

    return meta;
}

// Compute expected size for a piece index
static size_t expectedPieceSize(const DownloadMetadata &meta, size_t pieceIndex) {
    if (meta.numChunks == 0) return 0;
    if (pieceIndex + 1 < meta.numChunks) return CHUNK;
    // last piece:
    size_t last = meta.fileSize - (meta.numChunks - 1) * CHUNK;
    return last;
}

// Robust requestPiece: read exactly expectedSize bytes (or fail)
static bool requestPiece(const string &peer,
                         const string &fileName,
                         int pieceIndex,
                         const string &expectedHash,
                         size_t expectedSize,
                         int fd) {
    if (expectedSize == 0) return false;

    size_t colon = peer.find(':');
    if (colon == string::npos) return false;

    string ip = peer.substr(0, colon);
    int port = stoi(peer.substr(colon + 1));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[CLIENT] socket");
        return false;
    }

    // set a recv timeout so stuck peers don't hang forever
    struct timeval tv;
    tv.tv_sec = 8; // 8s timeout (tunable)
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    // Send request: "GET_PIECE <fileName> <index>\n"
    string req = "GET_PIECE " + fileName + " " + to_string(pieceIndex) + "\n";
    ssize_t s = send(sock, req.c_str(), req.size(), 0);
    if (s < 0) {
        perror("[CLIENT] send");
        close(sock);
        return false;
    }

    vector<uint8_t> buf(expectedSize);
    ssize_t total = 0;
    while (total < (ssize_t)expectedSize) {
        ssize_t r = recv(sock, reinterpret_cast<char*>(buf.data()) + total, expectedSize - total, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            // timeout or other recv error
            // log and fail so caller retries other peers
            // perror("[CLIENT] recv");
            close(sock);
            return false;
        }
        if (r == 0) break; // peer closed
        total += r;
    }

    close(sock);

    if ((size_t)total != expectedSize) {
        // incomplete piece — treat as failure so it will be retried
        cerr << "[CLIENT] Piece " << pieceIndex << " from " << peer
             << " returned " << total << " bytes, expected " << expectedSize << endl;
        return false;
    }

    // If the peer returned an error string (text beginning with "ERR"), detect it
    if (total >= 3 && buf[0] == 'E' && buf[1] == 'R' && buf[2] == 'R') {
        string errMsg(reinterpret_cast<char*>(buf.data()), (size_t)min(total, (ssize_t)1024));
        cerr << "[CLIENT] Peer " << peer << " error for piece " << pieceIndex << ": " << errMsg << endl;
        return false;
    }

    // Compute SHA1 on the exact bytes received
    char hashHex[41];
    computeSHA1(reinterpret_cast<char*>(buf.data()), expectedSize, hashHex);
    string gotHash(hashHex);

    if (gotHash != expectedHash) {
        cerr << "[CLIENT] Hash mismatch for piece " << pieceIndex
             << " from peer " << peer << " expected " << expectedHash
             << " got " << gotHash << endl;
        return false;
    }

    // Write atomically at offset using pwrite to avoid races between threads
    off_t offset = (off_t)pieceIndex * CHUNK;
    ssize_t written = 0;
    const uint8_t *writePtr = buf.data();
    while (written < (ssize_t)expectedSize) {
        ssize_t w = pwrite(fd, writePtr + written, expectedSize - written, offset + written);
        if (w < 0) {
            if (errno == EINTR) continue;
            perror("[CLIENT] pwrite");
            return false;
        }
        written += w;
    }

    return true;
}


// ------------------- Worker + Context -------------------

struct DownloadContext {
    queue<size_t> workQueue;
    mutex queueMutex;
    mutex coutMutex;
    mt19937 rng;
    const DownloadMetadata *meta;
    int fd;
    map<size_t,int> retryCount;
    vector<bool> pieceDone;
};

static void downloadWorker(int tid, DownloadContext *ctx) {
    while (true) {
        size_t pieceIndex;

        // Take next piece index from queue
        {
            lock_guard<mutex> lock(ctx->queueMutex);
            if (ctx->workQueue.empty()) return;
            pieceIndex = ctx->workQueue.front();
            ctx->workQueue.pop();

            if (ctx->pieceDone[pieceIndex]) {
                continue; // already done
            }
        }

        bool success = false;

        // Shuffle peers
        vector<string> peers = ctx->meta->peers;
        shuffle(peers.begin(), peers.end(), ctx->rng);

        size_t expectedSize = expectedPieceSize(*ctx->meta, pieceIndex);

        for (auto &peer : peers) {
            if (requestPiece(peer,
                             ctx->meta->fileName,
                             (int)pieceIndex,
                             ctx->meta->pieceHashes[pieceIndex],
                             expectedSize,
                             ctx->fd)) {
                {
                    lock_guard<mutex> lock(ctx->queueMutex);
                    ctx->pieceDone[pieceIndex] = true; // mark done
                }
                {
                    lock_guard<mutex> lock(ctx->coutMutex);
                    cout << "[CLIENT] Worker " << tid
                         << " got piece " << (pieceIndex + 1)
                         << "/" << ctx->meta->numChunks
                         << " from " << peer << endl;
                }
                success = true;
                break;
            }
        }

        if (!success) {
            lock_guard<mutex> lock(ctx->queueMutex);
            int count = ++ctx->retryCount[pieceIndex];
            if (!ctx->pieceDone[pieceIndex]) { // only retry if not already done
                if (count <= 3) {
                    ctx->workQueue.push(pieceIndex);
                    lock_guard<mutex> lock2(ctx->coutMutex);
                    cerr << "[CLIENT] Worker " << tid
                         << " requeued piece " << pieceIndex
                         << " (retry " << count << ")" << endl;
                } else {
                    lock_guard<mutex> lock2(ctx->coutMutex);
                    cerr << "[CLIENT] Worker " << tid
                         << " gave up on piece " << pieceIndex
                         << " after " << count << " attempts" << endl;
                }
            }
        }
    }
}

// ------------------- Main Download Function -------------------

bool startDownload(const DownloadMetadata &meta, const string &destPath) {
    int fd = open(destPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("[CLIENT] open dest");
        return false;
    }
    if (ftruncate(fd, meta.fileSize) < 0) {
        perror("[CLIENT] ftruncate");
        close(fd);
        return false;
    }

    DownloadContext ctx;
    ctx.meta = &meta;
    ctx.fd = fd;
    ctx.rng.seed(random_device{}());
    ctx.pieceDone.assign(meta.numChunks, false);

    for (size_t i = 0; i < meta.numChunks; i++) {
        ctx.workQueue.push(i);
    }

    int numThreads = min((size_t)8, meta.numChunks);
    vector<thread> workers;
    for (int i = 0; i < numThreads; i++) {
        workers.emplace_back(downloadWorker, i, &ctx);
    }

    for (auto &t : workers) {
        t.join();
    }

    close(fd);

    if (!verifyFullFile(meta, destPath)) {
        cerr << "[CLIENT] Download failed: file hash mismatch" << endl;
        return false;
    }
    cout << "[CLIENT] Download complete: " << destPath << endl;
    return true;
}
