#ifndef SEEDS_H
#define SEEDS_H

#include <map>
#include <string>
#include "uploader.h"



// Return absolute path for a user's seeds file
std::string getSeedsFilePath(const std::string &userId);

// Save seeds for a user
void saveSeeds(const std::string &userId,
               const std::map<std::string, SeedInfo> &seedingFiles);

// Load seeds for a user
std::map<std::string, SeedInfo> loadSeeds(const std::string &userId);

#endif
