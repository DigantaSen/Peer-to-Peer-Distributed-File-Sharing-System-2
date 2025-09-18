#include "commands.h"
#include "client.h"
#include "../../common/include/protocol.h"
#include <iostream>
#include <sstream>
using namespace std;

int handleCommand(Client &client, const string &input) {
    istringstream iss(input);
    string cmd;
    iss >> cmd;

    vector<string> args;
    string arg;
    while(iss >> arg) args.push_back(arg);

    if (cmd == CMD_CREATE_USER) {
        if (args.size() < 2) {
            cerr << "Usage: create_user <user> <pass>\n";
            printf("sss\n");
            return -1;
        }
        client.sendMessage(string(CMD_CREATE_USER) + " " + args[0] + " " + args[1]);
    }
    else if (cmd == CMD_LOGIN) {
        if (args.size() < 2) {
            cerr << "Usage: login <user> <pass>\n";
            return -1;
        }
        client.sendMessage(string(CMD_LOGIN)  + " " + args[0] + " " + args[1]);
    }
    else if (cmd == CMD_CREATE_GROUP) {
        if (args.empty()) {
            cerr << "Usage: create_group <group_id>\n";
            return -1;
        }
        client.sendMessage(string(CMD_CREATE_GROUP) + " " + args[0]);
    }
    else if (cmd == CMD_LIST_GROUPS) {
        client.sendMessage(string(CMD_LIST_GROUPS));
    }
    else if (cmd == CMD_JOIN_GROUP) {
        if (args.empty()) {
            cerr << "Usage: join_group <group_id>\n";
            return -1;
        }
        client.sendMessage(string(CMD_JOIN_GROUP) + " " + args[0]);
    }

    else if (cmd == CMD_UPLOAD_FILE) {
        if (args.size() < 2) {
            cerr << "Usage: upload_file <group_id> <file_path>\n";
            return -1;
        }
        client.sendMessage(string(CMD_UPLOAD_FILE) + " " + args[0] + " " + args[1]);
    }
    else if (cmd == CMD_LIST_REQUESTS) {
        if (args.empty()) {
            cerr << "Usage: list_requests <group_id>\n";
            return -1;
        }
        client.sendMessage(string(CMD_LIST_REQUESTS) + " " + args[0]);
    }
    else if (cmd == CMD_ACCEPT_REQUEST) {
        if (args.size() < 2) {
            cerr << "Usage: accept_request <group_id> <user_id>\n";
            return -1;
        }
        client.sendMessage(string(CMD_ACCEPT_REQUEST) + " " + args[0] + " " + args[1]);
    }
    else if (cmd == CMD_LEAVE_GROUP) {
        if (args.empty()) {
            cerr << "Usage: leave_group <group_id>\n";
            return -1;
        }
        client.sendMessage(string(CMD_LEAVE_GROUP) + " " + args[0]);
    }
    else if (cmd == CMD_LOGOUT) {
        client.sendMessage(string(CMD_LOGOUT));
    }

    else if (cmd == CMD_EXIT) {
        cout<<"[CLIENT] Exiting...\n";
        return 1;
    }
    else {
        cerr<<"[CLIENT] Unknown command: "<<cmd<<"\n";
        return -1;
    }

    return 0;
}
