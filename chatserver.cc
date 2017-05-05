#include "cs_common.h"
#include "cs_client.h"

// global variables
int sockfd;
int nn;
int N;

// server data
vector<address> bindAddresses;
vector<address> forwAddresses;

// client data 
map<address,clientInfo> clients; // client addresses to nicknames 
map<int, vector<address> > chatrooms;

// fifo ordering data
map<string, fifoQueue> fifoQueueMap;

// total ordering data
map<int, total_s> totalSenderMap; // chatroom to info
map<int, total_r> totalReceiverMap; // chatroom to info

int debug_mode = 0;
int order_mode = 0;
address selfAddr;

// method signatures
void runServer();
void forwardToServers(string const &msg);
void b_deliver(int roomId, string const &msg);
void fifo_deliver(string msg);
void total_sendInitial(int roomId);
void total_handle(struct address sender, string msg);
void total_updateReceiverQueue(int roomId, int T, int oldP);
void unordered_deliver(string const &msg);
void handleExistingClient(address client, string msg);
void handleNewClient(address client, string msg);

static inline std::string &rtrim(std::string &s) {
    s.erase(find_if(s.rbegin(), s.rend(),
                std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

//===== MAIN METHOD =======
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "*** Author: Selina Liu (liu15)\n");
        exit(1);
    }
    int c;
    while ((c = getopt(argc, argv, "o:v")) != -1) {
        switch (c) {
        case 'o':
            if (strcmp(optarg,"unordered") == 0) {
                order_mode = 0;
                if(debug_mode) debug_msg("Mode is Unordered");
            } else if (strcmp(optarg,"fifo") == 0) {
                order_mode = 1;
                if(debug_mode) debug_msg("Mode is Fifo Ordering");
            } else if (strcmp(optarg,"total") == 0) {
                order_mode = 2;
                if(debug_mode) debug_msg("Mode is Total Ordering");
            } else {
                throwMyError("Not a valid option");
            }
            break;
        case 'v':
            debug_mode = 1; // turn on debug mode 
            break;
        default:
            throwMyError("Not a valid option");
        }
    }

    if (argv[optind] == NULL) throwMyError("No file and server index specified");
    if (argv[optind+1] == NULL) throwMyError("No server index specified");

    const char *filename = argv[optind];
    nn = atoi(argv[optind+1]);
    populateServers(forwAddresses, bindAddresses, filename);
    runServer();
    return 0;
}

void runServer() {
    selfAddr = {forwAddresses[nn-1].addr, forwAddresses[nn-1].port};
    int status;
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = bindAddresses[nn-1].port;
    servaddr.sin_addr.s_addr = bindAddresses[nn-1].addr;
    status = ::bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    if (status < 0) {
        throwSysError("Socket did not bind");
    }
    string addtext = formatAddress(bindAddresses[nn-1]);
    if (debug_mode) debug_msg("Server bound to", addtext.c_str());

    char buffer[100];
    bzero(buffer, sizeof(buffer));
    
    // Main receiving loop 
    while (true) {
        struct sockaddr_in src;
        socklen_t srcSize = sizeof(src);
        bzero(&src, sizeof(src));

        int rlen = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0,
                            (struct sockaddr*) &src, &srcSize);
        buffer[rlen] = 0;
        string msg(buffer);
        
        struct address item = {
            src.sin_addr.s_addr,
            src.sin_port
        };
        
        // if from another server
        vector<address>::iterator it = find(forwAddresses.begin(), forwAddresses.end(), item);
        if (it != forwAddresses.end()) {
            int serverIndex = it-forwAddresses.begin();
            if (order_mode == 0) {
                unordered_deliver(msg);
            } else if (order_mode == 1) {
                fifo_deliver(msg);
            } else if (order_mode == 2) {
                total_handle(item, msg);
            }
        } 
        // from an existing client
        else if (clients.find(item) != clients.end()) {
            handleExistingClient(item, msg);
        }
        // from a new client
        else {
            handleNewClient(item, msg);
        }
        
        bzero(buffer, sizeof(buffer));
    }
}

void client_message(address client, string msg) {
    int currRoomId = clients[client].roomId;
    if (currRoomId == 0) {
        sendResponse(client, "-ERR Please join a room first");
        return;
    }
    string name = clients[client].nickname;
    string id = clients[client].id;
    int count = ++(clients[client].counts[currRoomId-1]);
    msg = "<" + name + "> " + msg;
    string basicPrefix = to_string(currRoomId) + ","; // room id
    string localMsg = basicPrefix + msg;

    if (debug_mode) debug_msg("Local client sent:", msg.c_str());
    
    if (order_mode == 0) {  // unordered
        unordered_deliver(localMsg);
        forwardToServers(localMsg);
    } else if (order_mode == 1) { // fifo ordering
        unordered_deliver(localMsg);
        string fifoPrefix = to_string(count) + "," + clients[client].id + ",";
        string finalMsg = fifoPrefix + localMsg;
        forwardToServers(finalMsg);
    } else if (order_mode == 2) { // total ordering
        totalSenderMap[currRoomId].msgQueue.push(msg);
        total_sendInitial(currRoomId);
    }
}

// Add new client to list of active clients 
void handleNewClient(address client, string msg) {
        rtrim(msg);
        if (msg.substr(0,6) != "/join ") {
            sendResponse(client, "-ERR please join a room first");
            return;
        }
        assert (msg.substr(0,6) == "/join ");
        
        int roomId = stoi(msg.substr(6));
        in_addr wrapper; wrapper.s_addr = client.addr;
        string addrId = inet_ntoa(wrapper) + string(":") + to_string(ntohs(client.port));
        
        // add client to list of clients
        struct clientInfo initInfo = { addrId, addrId, roomId, 0};
        clients[client] = initInfo; 
        chatrooms[roomId].push_back(client);
        
        sendResponse(client, "+OK You are now in chat room #", roomId);
        if (debug_mode) debug_msg("New client joined room #", roomId);
}

void handleExistingClient(address client, string msg) {
    rtrim(msg);
    int currRoomId = clients[client].roomId;
    if (msg == "/join" || msg == "/nick") {
        sendResponse(client, "-ERR You need an argument");

    } else if (msg.substr(0,6) == "/join ") {
        int newroom = stoi(msg.substr(6));
        client_join(client, newroom);
    } else if (msg.substr(0,5) == "/part") {
        client_part(client);
    } else if (msg.substr(0,6) == "/nick ") {
        string name = msg.substr(6);
        client_nick(client, name);
    } else if (msg.substr(0,5) == "/quit") {
        client_quit(client);
    } 
    else { // client sends a message to the group
        client_message(client, msg);
    }
}

// Message format: <msgId>,<clientId>,<roomId>,message
void fifo_deliver(string msg) {
    size_t pos1, pos2, pos3;
    pos1 = msg.find(comma);
    pos2 = msg.find(comma, pos1+1);
    pos3 = msg.find(comma, pos2+1);

    int msgId = stoi(msg.substr(0,pos1));
    string clientId = msg.substr(pos1+1,pos2-pos1-1);
    int roomId = stoi(msg.substr(pos2+1,pos3-pos2-1));
    // queueId = clientId,roomId
    string queueId = msg.substr(pos1+1,pos3-pos1-1);
    string origMsg = msg.substr(pos3+1);
    fifoQueue &fq = fifoQueueMap[queueId];
    int r = fq.lastMsgId;
    if (msgId == (r+1)) {
        b_deliver(roomId, origMsg);
        fq.lastMsgId++;
        if (debug_mode) debug_msg("Received and delivered MSG #", msgId); 
        
        // look at top of queue
        while (!fq.queue.empty()) {
            const fifoMsg &curr = fq.queue.top();

            if (curr.id == fq.lastMsgId+1) {
                b_deliver(roomId, curr.msg);
                fq.lastMsgId++;
                if (debug_mode) debug_msg("Popped and delivered from queue MSG #", curr.id); 
                fq.queue.pop();
            } else {
                if (debug_mode) debug_msg("Need earlier MSG to arrive");
                break;
            }
        }
    } else if (msgId > (r+1)) {
        fifoMsg item = {msgId, origMsg};
        fq.queue.push(item);
        if (debug_mode) debug_msg("Pushed to queue MSG #", msgId);
    } 
    else {
        if (debug_mode) debug_msg("MSG received has been delivered");
    }
}

void total_sendInitial(int roomId) {
    total_s &sInfo = totalSenderMap[roomId];
    // if busy, sender is still waiting for all responses to come in
    if (sInfo.busy || sInfo.msgQueue.empty()) return;
    // assert (sInfo.busy == false);
    // assert (sInfo.responses.empty()); 
    string msg = to_string(roomId) + comma + sInfo.msgQueue.front();
    // update own receiverMap as well
    total_r &rInfo = totalReceiverMap[roomId];
    rInfo.P++;
    totalMsg foo = {rInfo.P, selfAddr, sInfo.msgQueue.front(), false};
    rInfo.queue.push_back(foo);

    sInfo.T = max(sInfo.T, rInfo.P);
    sInfo.responses[selfAddr] = rInfo.P;
    // forward initial msg to other servers
    forwardToServers(msg);
    sInfo.busy = true; // waiting for responses from all servers to come in
}


void total_sendFinal(int roomId) {
    // after sending final msg, need to send initial msg for next msg in line
    total_s &currInfo = totalSenderMap[roomId];
    map<address,int> &res = currInfo.responses;
    map<address, int>::iterator it;
    string rawMsg = currInfo.msgQueue.front();
    int status;
    string msg;
    for (it = res.begin(); it != res.end(); it++) {
        if (it->first == selfAddr) {
            total_updateReceiverQueue(roomId, currInfo.T, it->second);
            continue;
        }
        struct sockaddr_in curr;
        bzero(&curr, sizeof(curr));
        curr.sin_family = AF_INET;
        curr.sin_port = it->first.port;
        curr.sin_addr.s_addr = it->first.addr;
        
        msg = "T" + to_string(currInfo.T) + comma + to_string(it->second) + 
                        comma + to_string(roomId) + comma + rawMsg;
        
        status = sendto(sockfd, msg.c_str(), msg.size(), 0,
                    (struct sockaddr*) &curr, sizeof(curr));
        if (status < 0 && debug_mode) debug_msg("Error sending packet");
    }
    debug_msg("Sent out final message:", msg.c_str());
    currInfo.msgQueue.pop();
    currInfo.responses.clear();
    assert (currInfo.responses.empty());
    currInfo.T = 0;
    currInfo.busy = false; // not waiting for responses anymore
    // move on to process the next unsent message in queue
    if (!currInfo.msgQueue.empty())  total_sendInitial(roomId);
}

void total_updateReceiverQueue(int roomId, int T, int oldP) {
    total_r &currInfo = totalReceiverMap[roomId];
    vector<totalMsg> &queue = currInfo.queue;
    currInfo.A = max(currInfo.A, T);
    if (oldP <= T) {
        // update queue
        // find totalMsg with oldP as timestamp and update
        for (int i = 0; i < queue.size(); i++) {
            if (!queue[i].deliverable && queue[i].timestamp == oldP) {
                // cout << "QUEUE (before update)" << endl;
                // printHoldbackQueue(queue);
                queue[i].deliverable = true;
                queue[i].timestamp = T;
                sort(queue.begin(), queue.end());
                // cout << "QUEUE (after update)" << endl;
                // printHoldbackQueue(queue);
                if (debug_mode) {
                    debug_msg("Reordered holdback queue for chatroom #", roomId);
                }
                break;
            }
        }
    } else {
        if (debug_mode) debug_msg("ERROR! This shouldn't happen!");
    }
    while (!queue.empty()) {
        if (queue[0].deliverable) {
            totalMsg &front = queue[0];
            string fmsg = front.msg;
            b_deliver(roomId, fmsg);
            queue.erase(queue.begin());
            if (debug_mode) debug_msg("Delivered front of holdback queue:", 
                                    fmsg.c_str());
        } else {
            if (debug_mode) debug_msg("First message in holdback queue not yet deliverable");
            break;
        }
    }
}

void total_handle(struct address sender, string msg) {
    //todo: check for empty string
    string text;
    int pos, roomId;
    // sender receiving proposal from receivers
    if (msg[0] == 'P') {
        if (debug_mode) debug_msg("Got proposal: ", msg.c_str());
        pos = msg.find(comma);
        int P = stoi(msg.substr(1,pos));
        roomId = stoi(msg.substr(pos+1));
        total_s &currInfo = totalSenderMap[roomId];
        map<address,int> &currResponses = currInfo.responses;
        if (currResponses.find(sender) == currResponses.end()) {
            currResponses[sender] = P;
            currInfo.T = max(currInfo.T, P);
            // send out final timestamp 
            if (currResponses.size() == N) total_sendFinal(roomId); 
        } else {
            if (debug_mode) debug_msg("This server already proposed");
        }
    } 
    // receivers receiving final timestamp from sender
    else if (msg[0] == 'T') {
        if (debug_mode) debug_msg("Got final message: ", msg.c_str());
        // parsing final message from sender
        int pos1 = msg.find(comma);
        int pos2 = msg.find(comma, pos1+1);
        int pos3 = msg.find(comma, pos2+1);
        int T = stoi(msg.substr(1,pos1));
        int P = stoi(msg.substr(pos1+1, pos2-pos1-1));
        roomId = stoi(msg.substr(pos2+1, pos3-pos2-1));
        text = msg.substr(pos3+1);
        total_updateReceiverQueue(roomId, T, P);
    } 
    // receivers receiving initial message from sender
    else { 
        if (debug_mode) debug_msg("Got initial message: ", msg.c_str());
        pos = msg.find(comma);
        roomId = stoi(msg.substr(0,pos));
        text = msg.substr(pos+1);
        total_r &currInfo = totalReceiverMap[roomId];

        //todo: send proposal message back to sender
        currInfo.P = max(currInfo.P, currInfo.A) + 1;
        string proposal = "P" + to_string(currInfo.P) + comma + to_string(roomId); 
        
        struct sockaddr_in curr;
        bzero(&curr, sizeof(curr));
        curr.sin_family = AF_INET;
        curr.sin_port = sender.port;
        curr.sin_addr.s_addr = sender.addr;
        int status = sendto(sockfd, proposal.c_str(), proposal.size(), 0,
                    (struct sockaddr*) &curr, sizeof(curr));
        if (status < 0 && debug_mode) debug_msg("Error sending packet");
        if (debug_mode) debug_msg("Proposed ", currInfo.P);
        totalMsg tm = { currInfo.P, sender, text, false };
        currInfo.queue.push_back(tm);
    }
}

// forward to all clients in the chat room 
void unordered_deliver(string const &msg) {
    size_t pos = msg.find(",");
    int roomId = stoi(msg.substr(0,pos));
    string finalmsg = msg.substr(pos+1);
    b_deliver(roomId, finalmsg);
}

// basic local deliver primitive
void b_deliver(int roomId, string const &msg) {
    vector<address> &list = chatrooms[roomId];
    int status;
    for (int i = 0; i < list.size(); i++) {
        struct sockaddr_in curr;
        bzero(&curr, sizeof(curr));
        curr.sin_family = AF_INET;
        curr.sin_port = list[i].port;
        curr.sin_addr.s_addr = list[i].addr;
        status = sendto(sockfd, msg.c_str(), msg.size(), 0,
                    (struct sockaddr*) &curr, sizeof(curr));
        if (status < 0 && debug_mode) {
            debug_msg("Error delivering packet to clients");
        }
    }
}

// forward msg to all other servers except self
void forwardToServers(string const &msg) {
    if (debug_mode) debug_msg("Forwarding to other servers:", msg.c_str());
    int status;
    for (int i = 0; i < forwAddresses.size(); i++) {
        if ((i+1) == nn) continue;
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = forwAddresses[i].port;
        addr.sin_addr.s_addr = forwAddresses[i].addr;
        status = sendto(sockfd, msg.c_str(), msg.size(), 0, 
                (struct sockaddr*) &addr, sizeof(addr));
        if (status < 0 && debug_mode) {
            debug_msg("Error forwarding packet to other servers");
        }
    }
}
