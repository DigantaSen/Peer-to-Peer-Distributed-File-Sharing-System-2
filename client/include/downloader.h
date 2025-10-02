#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <vector>
using namespace std;

class Client;

struct DownloadMetadata {
    std::string fileName;
    size_t fileSize;
    size_t numChunks;
    std::string fullHash;
    std::string groupId;
    std::vector<std::string> pieceHashes;
    std::vector<std::string> peers;
};

DownloadMetadata parseDownloadResponse(const std::string &response);
bool startDownload(Client &client, const DownloadMetadata &meta, const std::string &destPath);
void showDownloads(const string &userId);
void closeDownloadLog();

#endif
