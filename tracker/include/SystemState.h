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
    std::vector<std::string> memberOrder; 
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
    bool joinGroup(const std::string &gid, const std::string &userId, std::string &errMsg);
    std::vector<std::string> listRequests(const std::string &gid, const std::string &owner, std::string &errMsg);
    bool acceptRequest(const std::string &gid, const std::string &owner, const std::string &userId, std::string &errMsg);
    bool leaveGroup(const std::string &gid, const std::string &userId, std::string &errMsg);
    bool logoutUser(const std::string &userId, std::string &errMsg);

    std::map<std::string, User> users;
    std::map<std::string, Group> groups;
    std::map<std::string, std::map<std::string, FileMetadata>> groupFiles;
};

#endif
