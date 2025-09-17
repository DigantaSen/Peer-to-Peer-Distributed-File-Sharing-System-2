#include "../include/SystemState.h"
#include <mutex>

static std::mutex stateMutex;

bool SystemState::createUser(const std::string &userId, const std::string &password, std::string &errMsg) {
    if (users.find(userId) != users.end()) {
        errMsg = "User already exists";
        return false;
    }
    users[userId] = User{userId, password, false};
    return true;
}

bool SystemState::loginUser(const std::string &userId, const std::string &password, std::string &errMsg) {
    auto it = users.find(userId);
    if (it == users.end()) {
        errMsg = "User does not exist";
        return false;
    }
    if (it->second.passwordHash != password) {
        errMsg = "Incorrect password";
        return false;
    }
    it->second.loggedIn = true;
    return true;
}

bool SystemState::createGroup(const std::string &gid, const std::string &owner, std::string &errMsg) {
    if (groups.find(gid) != groups.end()) {
        errMsg = "Group already exists";
        return false;
    }
    Group g;
    g.groupId = gid;
    g.ownerId = owner;
    g.members.insert(owner);
    groups[gid] = g;
    return true;
}

std::vector<std::string> SystemState::listGroups() {
    std::lock_guard<std::mutex> lock(stateMutex);
    std::vector<std::string> res;
    for (auto &entry : groups) {
        res.push_back(entry.first);
    }
    return res;
}
