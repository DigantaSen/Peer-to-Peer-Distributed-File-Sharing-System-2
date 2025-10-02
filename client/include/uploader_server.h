#ifndef UPLOADER_SERVER_H
#define UPLOADER_SERVER_H

#include <string>
#include <map>
#include <mutex>
#include "seeds.h"   // ✅ for SeedInfo

extern std::mutex seedingFilesMutex;
extern std::map<std::string, std::map<std::string, SeedInfo>> userSeeds;

bool startUploaderServer(const std::string &ip, int port, const std::string &userId);
void stopUploaderServer();
void closeUploaderLog();

#endif
