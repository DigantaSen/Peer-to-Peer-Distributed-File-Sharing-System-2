#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <vector>

struct DownloadMetadata {
    std::string fileName;
    size_t fileSize;
    size_t numChunks;
    std::string fullHash;
    std::vector<std::string> pieceHashes;
    std::vector<std::string> peers;
};

DownloadMetadata parseDownloadResponse(const std::string &response);
bool startDownload(const DownloadMetadata &meta, const std::string &destPath);

#endif
