#ifndef STATE_H
#define STATE_H

#include <string>
#include <map>
#include <set>

struct User {
    std::string userId;
    std::string passwordHash;
    bool loggedIn = false;
};

struct Group {
    std::string groupId;
    std::string ownerId;
    std::set<std::string> members;
    std::set<std::string> pendingRequests;
};

struct FileMetadata {
    std::string fileName;
    std::string fileHash;
    std::set<std::string> peers;
};

struct SystemState {
    std::map<std::string, User> users;
    std::map<std::string, Group> groups;
    std::map<std::string, std::map<std::string, FileMetadata>> groupFiles;
};


#endif
