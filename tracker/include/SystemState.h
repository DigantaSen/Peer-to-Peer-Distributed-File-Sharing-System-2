#ifndef SYSTEMSTATE_H
#define SYSTEMSTATE_H

#include <string>
#include <vector>
#include <map>
#include <set>

struct User {
    std::string userId;
    std::string passwordHash;
    bool loggedIn;

    User() : loggedIn(false) {}  // default constructor
    User(const std::string& id, const std::string& pass, bool logged = false)
        : userId(id), passwordHash(pass), loggedIn(logged) {}
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

class SystemState {
public:
    bool createUser(const std::string &userId, const std::string &password, std::string &errMsg);
    bool loginUser(const std::string &userId, const std::string &password, std::string &errMsg);
    bool createGroup(const std::string &gid, const std::string &owner, std::string &errMsg);
    std::vector<std::string> listGroups();

    std::map<std::string, User> users;
    std::map<std::string, Group> groups;
    std::map<std::string, std::map<std::string, FileMetadata>> groupFiles;
};

#endif
