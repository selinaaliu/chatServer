#ifndef __cs_common_h_
#define __cs_common_h_

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <set>
#include <queue>
#include <map> 
#include <tuple>
#include <assert.h>

using namespace std;

#define comma ","

struct address {
    uint32_t addr;
    uint16_t port;
};

struct clientInfo {
    string id; // address and port of client
    string nickname;
    int roomId;
    int counts[10]; // count of messages sent so far
};

struct fifoMsg {
    int id;
    string msg;
};

struct fifoQueue {
    int lastMsgId;
    priority_queue<fifoMsg> queue;
};

struct totalMsg {
    int timestamp;
    address node;
    string msg;
    bool deliverable;
};

// data structure for the sender side
struct total_s {
    map<address, int> responses;
    int T;
    queue<string> msgQueue;
    bool busy = false;
};

// data structure for the recipient side
struct total_r {
    int P; // highest proposed number
    int A; // highest agreed number seen so far
    vector<totalMsg> queue;
};

bool operator < (const address &a, const address &b);
bool operator == (const address &a, const address &b);
bool operator < (const totalMsg &a, const totalMsg &b);
bool operator < (const fifoMsg &a, const fifoMsg &b);

// GLOBAL VARIABLES
extern int N; // total number of servers in the list of servers
extern int nn; // index of this server in list of servers
extern int sockfd;

extern int debug_mode;
extern map<address,clientInfo> clients; // client addresses to nicknames 
extern map<int, vector<address> > chatrooms;

// METHODS 
void populateServers(vector<address> &f, vector<address> &b, const char* filename);
void throwSysError(const char* msg);
void throwMyError(const char* msg);
void sendResponse(struct address client, const char *msg);
void sendResponse(struct address client, const char *msg, int val);
void sendResponse(struct address client, const char *msg, const char *val);
void debug_msg(const char* msg);
void debug_msg(const char* msg, int val);
void debug_msg(const char* msg, const char *val);
string getFormattedTime();
string formatAddress(address input);
void printServers(set<address> &servers);
void printHoldbackQueue(vector<totalMsg> &queue);
void printClientStats();
#endif
