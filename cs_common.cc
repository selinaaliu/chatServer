#include "cs_common.h"

bool operator < (const address &a, const address &b) {
    return tie(a.addr, a.port) < tie(b.addr, b.port);
}

bool operator == (const address &a, const address &b) {
    return a.addr == b.addr && a.port == b.port;
}

bool operator < (const totalMsg &a, const totalMsg &b) {
    return tie(a.timestamp, a.node) < tie(b.timestamp, b.node);
}

bool operator < (const fifoMsg &a, const fifoMsg &b) {
    return a.id > b.id;
}

void populateServers(vector<address> &f, vector<address> &b, const char* filename) {
    ifstream infile(filename);
    string line;
    size_t com, col;
    string::size_type sz;
    while (getline(infile, line)) {
        N++;
        struct address curr;
        com= line.find(",");
        col= line.find(":");
        curr.addr = inet_addr(line.substr(0,col).c_str());
        curr.port = htons(stoi(line.substr(col+1,com-col-1), &sz));
        f.push_back(curr);
        struct address bind;
        if (com!= string::npos) { // if there are two addresses
            col= line.find(":", col+1);
            bind.addr = inet_addr(line.substr(com+1,col-com-1).c_str());
            bind.port = htons(stoi(line.substr(col+1), &sz));
        } 
        else {
            bind.addr = curr.addr;
            bind.port = curr.port;
        }
        b.push_back(bind);
    }
}

// server sends response to client
void sendResponse(struct address client, const char *msg, int val) {
    struct sockaddr_in addr; bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = client.addr;
    addr.sin_port = client.port;
    char buf[strlen(msg) + 3];
    sprintf(buf, "%s%d", msg, val);
    int status = sendto(sockfd, buf, strlen(buf), 0, 
                    (struct sockaddr*) &addr, sizeof(addr));
    if (status < 0) throwSysError("Error sending response to client");
}

void sendResponse(struct address client, const char *msg, const char *val) {
    struct sockaddr_in addr; bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = client.addr;
    addr.sin_port = client.port;
    char buf[strlen(msg) + strlen(val)];
    sprintf(buf, "%s %s", msg, val);
    int status = sendto(sockfd, buf, strlen(buf), 0, 
                    (struct sockaddr*) &addr, sizeof(addr));
    if (status < 0) throwSysError("Error sending response to client");
}

void sendResponse(struct address client, const char *msg) {
    struct sockaddr_in addr; bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = client.addr;
    addr.sin_port = client.port;
    int status = sendto(sockfd, msg, strlen(msg), 0, 
                    (struct sockaddr*) &addr, sizeof(addr));
    if (status < 0) throwSysError("Error sending response to client");
}

void debug_msg(const char* msg) {
    string time = getFormattedTime();
    printf("%s S%02d %s\n", time.c_str(), nn, msg);
}

void debug_msg(const char* msg, int val) {
    string time = getFormattedTime();
    printf("%s S%02d %s %d\n", time.c_str(), nn, msg, val);
}

void debug_msg(const char* msg, const char *val) {
    string time = getFormattedTime();
    printf("%s S%02d %s %s\n", time.c_str(), nn, msg, val);
}

void throwSysError(const char* msg) { perror(msg); exit(1); }
void throwMyError(const char* msg) { fprintf(stderr, "%s\n", msg); exit(1); }

string getFormattedTime() {
    char buffer1[80], buffer2[80];
    struct timeval rawtime;
    gettimeofday(&rawtime, NULL);
    struct tm *timeinfo = localtime(&rawtime.tv_sec);
    strftime(buffer1,80,"%R:%S",timeinfo);
    snprintf(buffer2,80, "%s.%06ld", buffer1, rawtime.tv_usec);
    string out(buffer2);
    return out;
}

string formatAddress(address input) {
    in_addr wrapper;
    wrapper.s_addr = input.addr;
    string out = inet_ntoa(wrapper) + string(":") + to_string(ntohs(input.port));
    return out;
}

void printServers(vector<address> &servers) {
    for (int i = 0; i < servers.size(); i++) {
        struct address curr = servers[i];
        cout << curr.addr << "::" << curr.port << endl;
    }
}

// debugging helper method
void printHoldbackQueue(vector<totalMsg> &queue) {
    cout << "---- start -----" << endl;
    cout << "timestamp | node | deliverable" << endl;
    for (int i = 0; i < queue.size(); i++) {
        totalMsg &curr = queue[i];
        cout << curr.timestamp << " | " 
            // << curr.node << " | "
            << "(" << curr.node.addr << "," << curr.node.port << ")" << " | "
            << curr.deliverable << " | " << curr.msg << endl;
    }
    cout << "---- end -----" << endl << endl;
}

void printClientStats() {
    cout << "total # of clients = " << clients.size() << endl;
    cout << "Room | # Clients" << endl;
    map<int,vector<address> >::iterator it;
    for (it = chatrooms.begin(); it != chatrooms.end(); it++) {
        cout << it->first << " | " << it->second.size() << endl;
    }
}

