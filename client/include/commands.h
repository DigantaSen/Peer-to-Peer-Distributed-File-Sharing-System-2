#ifndef COMMANDS_H
#define COMMANDS_H

#include <string>
#include <vector>

// ---- Function declaration ----
// returns true if client should exit
class Client;   // forward declaration
int handleCommand(Client &client, const std::string &input);

#endif // COMMANDS_H
