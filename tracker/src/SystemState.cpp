#include "../include/SystemState.h"
#include <mutex>
#include <algorithm>
using namespace std;

static mutex stateMutex;

bool SystemState::createUser(const string &userId, const string &password, string &errMsg) {
    if (users.find(userId) != users.end()) {
        errMsg = "User already exists";
        return false;
    }
    users[userId] = User{userId, password, false};
    return true;
}

bool SystemState::loginUser(const string &userId, const string &password, string &errMsg) {
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

bool SystemState::createGroup(const string &gid, const string &owner, string &errMsg) {
    if (groups.find(gid) != groups.end()) {
        errMsg = "Group already exists";
        return false;
    }
    Group g;
    g.groupId = gid;
    g.ownerId = owner;
    g.members.insert(owner);
    g.memberOrder.push_back(owner);

    groups[gid] = g;
    return true;
}

vector<string> SystemState::listGroups() {
    lock_guard<mutex> lock(stateMutex);
    vector<string> res;
    for (auto &entry : groups) {
        res.push_back(entry.first);
    }
    return res;
}

bool SystemState::joinGroup(const string &gid, const string &userId, string &errMsg) {
    auto it = groups.find(gid);
    if (it == groups.end()) {
        errMsg = "Group does not exist";
        return false;
    }

    Group &grp = it->second;

    if (grp.members.count(userId)) {
        errMsg = "Already a member of this group";
        return false;
    }

    if (grp.pendingRequests.count(userId)) {
        errMsg = "Join request already sent";
        return false;
    }

    grp.pendingRequests.insert(userId);
    return true;
}

vector<string> SystemState::listRequests(const string &gid, const string &owner, string &errMsg) {
    auto it = groups.find(gid);
    if (it == groups.end()) {
        errMsg = "Group does not exist";
        return {};
    }

    Group &grp = it->second;
    if (grp.ownerId != owner) {
        errMsg = "Only the group owner can view requests";
        return {};
    }

    vector<string> requests(grp.pendingRequests.begin(), grp.pendingRequests.end());
    return requests;
}

bool SystemState::acceptRequest(const string &gid, const string &owner, const string &userId, string &errMsg) {
    auto it = groups.find(gid);
    if (it == groups.end()) {
        errMsg = "Group does not exist";
        return false;
    }

    Group &grp = it->second;
    if (grp.ownerId != owner) {
        errMsg = "Only the group owner can accept requests";
        return false;
    }

    if (!grp.pendingRequests.count(userId)) {
        errMsg = "No such pending request from " + userId;
        return false;
    }

    grp.pendingRequests.erase(userId);
    grp.members.insert(userId);
    grp.memberOrder.push_back(userId);


    return true;
}

bool SystemState::leaveGroup(const string &gid, const string &userId, string &errMsg) {
    auto it = groups.find(gid);
    if (it == groups.end()) {
        errMsg = "Group does not exist";
        return false;
    }

    Group &grp = it->second;

    if (!grp.members.count(userId) && grp.ownerId != userId) {
        errMsg = "You are not a member of this group";
        return false;
    }

    if (grp.ownerId == userId) {
        string newOwner;

        for (auto &member : grp.memberOrder) {
            if (member != userId && grp.members.count(member)) {
                newOwner = member;
                break;
            }
        }

        if (!newOwner.empty()) {
            grp.ownerId = newOwner;

            grp.members.erase(userId);
            grp.memberOrder.erase(
                remove(grp.memberOrder.begin(), grp.memberOrder.end(), userId),
                grp.memberOrder.end()
            );

            return true;
        } else {
            groups.erase(it);
            return true;
        }
    }

    grp.members.erase(userId);
    grp.memberOrder.erase(
        remove(grp.memberOrder.begin(), grp.memberOrder.end(), userId),
        grp.memberOrder.end()
    );

    return true;
}

bool SystemState::logoutUser(const string &userId, string &errMsg) {
    auto it = users.find(userId);
    if (it == users.end()) {
        errMsg = "User does not exist";
        return false;
    }
    if (!it->second.loggedIn) {
        errMsg = "User is not logged in";
        return false;
    }

    it->second.loggedIn = false;
    return true;
}

bool SystemState::uploadFile(const string &gid,
                             const string &userId,
                             const string &fileName,
                             size_t fileSize,
                             size_t numChunks,
                             const string &fullHash,
                             const vector<string> &pieceHashes,
                             string &errMsg) {
    // Check group exists
    if (groups.find(gid) == groups.end()) {
        errMsg = "Group does not exist";
        return false;
    }

    // Check user is a member of the group
    if (groups[gid].members.find(userId) == groups[gid].members.end()) {
        errMsg = "User not a member of group " + gid;
        return false;
    }

    // Reference to this group's file map
    auto &files = groupFiles[gid];

    // If file already exists, add peer
    if (files.find(fileName) != files.end()) {
        files[fileName].peers.insert(userId);
        return true;
    }

    // Otherwise, create new file entry
    FileMetadata meta;
    meta.fileName = fileName;
    meta.fileSize = fileSize;
    meta.numChunks = numChunks;
    meta.fullHash = fullHash;
    meta.pieceHashes = pieceHashes;
    meta.peers.insert(userId);

    files[fileName] = meta;
    return true;
}

vector<string> SystemState::listFiles(const string &gid, string &errMsg) {
    auto it = groups.find(gid);
    if (it == groups.end()) {
        errMsg = "Group does not exist";
        return {};
    }

    vector<string> files;
    if (groupFiles.find(gid) != groupFiles.end()) {
        for (auto &entry : groupFiles[gid]) {
            files.push_back(entry.first);  // fileName
        }
    }
    return files;
}
