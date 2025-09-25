#ifndef UPLOADER_H
#define UPLOADER_H

#include <string>
#include <vector>

struct FileMetadata {
    std::string fileName;
    std::string filePath;
    size_t fileSize;
    size_t numChunks;
    std::string fullHash;
    std::vector<std::string> pieceHashes;
};

bool uploadFile(class Client &client, const std::string &filePath, const std::string &groupId);

#endif
