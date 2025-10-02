#include "../include/seeds.h"
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <sys/stat.h>

// Build absolute path for user seed file
std::string getSeedsFilePath(const std::string &userId) {
    const char *home = getenv("HOME");
    std::string base = (home ? home : ".");
    return base + "/.p2p_seeds_" + userId + ".txt";
}

// Save seeds: fileName path groupId
void saveSeeds(const std::string &userId,
               const std::map<std::string, SeedInfo> &seedingFiles) {
    std::string path = getSeedsFilePath(userId);
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("saveSeeds: open");
        return;
    }

    for (auto &entry : seedingFiles) {
        // Format: filename path groupId
        std::string line = entry.first + " " + entry.second.path + " " + entry.second.groupId + "\n";
        write(fd, line.c_str(), line.size());
    }
    close(fd);
}

// Load seeds: fileName path groupId
std::map<std::string, SeedInfo> loadSeeds(const std::string &userId) {
    std::map<std::string, SeedInfo> seeds;
    std::string path = getSeedsFilePath(userId);
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return seeds; // no seeds file yet

    char buf[4096];
    ssize_t n;
    std::string content;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        content.append(buf, n);
    }
    close(fd);

    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\n', pos);
        if (end == std::string::npos) end = content.size();
        std::string line = content.substr(pos, end - pos);
        if (!line.empty()) {
            std::istringstream iss(line);
            std::string fileName, filePath, groupId;
            if (iss >> fileName >> filePath >> groupId) {
                seeds[fileName] = {filePath, groupId};
            }
        }
        pos = end + 1;
    }
    return seeds;
}
