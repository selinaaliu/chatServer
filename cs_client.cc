#include "cs_client.h"

void client_quit(address client) {
    int currRoomId = clients[client].roomId;
    string clientId = clients[client].id;
    clients.erase(client);
    if (currRoomId > 0) {
        vector<address> &room = chatrooms[currRoomId];
        vector<address>::iterator it = find(room.begin(), room.end(), client);
        room.erase(it);
    }
    if (debug_mode) debug_msg("Client quitted", clientId.c_str());
} 

void client_part(address client) {
    int currRoomId = clients[client].roomId; 
    if (currRoomId == 0) {
        sendResponse(client, "-ERR you are not in a room yet");
        return;
    }
    vector<address> &room = chatrooms[currRoomId];
    vector<address>::iterator it = find(room.begin(), room.end(), client);
    room.erase(it);
    clients[client].roomId = 0;
    sendResponse(client, "+OK You have left chat room #", currRoomId);
    if (debug_mode) debug_msg("Client left chat room #", currRoomId);
}

void client_nick(address client, string const &name) {
    clients[client].nickname = name;
    sendResponse(client, "+OK Your new nickname is", name.c_str());
    if (debug_mode) debug_msg("Client changed nickname to", name.c_str());
}

void client_join(address client, int newRoomId) {
    int currRoomId = clients[client].roomId; 
    if (currRoomId != 0) {
        sendResponse(client, "-ERR you are already in room #", currRoomId);
        return;
    }
    clients[client].roomId = newRoomId;
    chatrooms[newRoomId].push_back(client);
    sendResponse(client, "+OK You are now in chat room #", newRoomId);
    if (debug_mode) debug_msg("Client joined chat room #", newRoomId);
}


