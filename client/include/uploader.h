#ifndef UPLOADER_H
#define UPLOADER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>

// Info for a seeding file
struct FileMetadata {
    std::string fileName;
    std::string filePath;
    size_t fileSize;
    size_t numChunks;
    std::string fullHash;
    std::vector<std::string> pieceHashes;
};

// SeedInfo to track both path and groupId
struct SeedInfo {
    std::string path;
    std::string groupId;
};

// userId -> (fileName -> SeedInfo)
extern std::map<std::string, std::map<std::string, SeedInfo>> userSeeds;

// Mutex will be defined in uploader.cpp or a common file
extern std::mutex seedingFilesMutex;

bool uploadFile(class Client &client,
                const std::string &filePath,
                const std::string &groupId);

#endif
