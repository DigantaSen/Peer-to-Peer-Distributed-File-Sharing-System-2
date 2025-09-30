#ifndef UPLOADER_SERVER_H
#define UPLOADER_SERVER_H

#include <string>

// Start the peer uploader server (runs in its own thread)
bool startUploaderServer(const std::string &ip, int port);

#endif
