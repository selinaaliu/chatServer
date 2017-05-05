#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>

using namespace std;
void verbatim(const char *prefix, const char *data, int len, const char *suffix);

void throwSysError(const char *msg) {
    perror(msg);
    exit(1);
}

void throwMyError(const char *msg) {
    printf("%s\n", msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "*** Author: Selina Liu (liu15)\n");
        exit(1);
    }

    // parsing command line inputs: address and port
    string input = argv[1];
    int pos = input.find(":");
    const char *ip_addr = input.substr(0,pos).c_str();
    int port = stoi(input.substr(pos+1));
    
    int status;
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) throwSysError("Failed to create socket");
    
    // this client's address
    struct sockaddr_in cliaddr;
    bzero(&cliaddr, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    status = ::bind(sockfd, (struct sockaddr*) &cliaddr, sizeof(cliaddr));
    if (status < 0) throwSysError("Failed to bind to port");

    // destination server's address
    struct sockaddr_in dest;
    bzero(&dest, sizeof(dest));
    socklen_t destSize = sizeof(dest);
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    //dest.sin_addr.s_addr = htons(inet_addr(ip_addr));
    inet_pton(AF_INET, ip_addr, &(dest.sin_addr));
    //dest.sin_addr.s_addr = htonl(INADDR_ANY);
    
    // cout << "client addr = " << inet_ntoa(cliaddr.sin_addr)
        // << ":" << ntohs(cliaddr.sin_port) << endl;

    // cout << "server addr = " << inet_ntoa(dest.sin_addr)
        // << ":" << ntohs(dest.sin_port) << endl;

    
    char buffer[100];
    char sbuffer[100];
    bzero(buffer, sizeof(buffer));
    bzero(sbuffer, sizeof(sbuffer));
    
    struct sockaddr_in src; // should be same as dest
    socklen_t srcSize = sizeof(src);

    while (true) {
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int nfds = sockfd + 1;

        // there is something to read 
        if (select(nfds, &readfds, NULL, NULL, NULL) > 0) {

            // read from server
            if (FD_ISSET(sockfd, &readfds)) {
                int rlen = recvfrom(sockfd, sbuffer, sizeof(sbuffer)-1, 0,
                                (struct sockaddr*)&src, &srcSize);
                printf("%s\n", sbuffer);
                bzero(sbuffer, sizeof(sbuffer));
            } 
            
            // read from user input 
            else if (FD_ISSET(STDIN_FILENO, &readfds)) {
                // read from user input and send to server
                while (read(STDIN_FILENO, buffer, sizeof(buffer)) > 0) {
                    status = sendto(sockfd, buffer, strlen(buffer)-1, 0, 
                                    (struct sockaddr*) &dest, sizeof(dest));
                    string msg(buffer);
                    bzero(buffer,strlen(buffer));
                    if (status < 0) throwSysError("Error sending packet");
                    else { 
                        if (msg.substr(0,5) == "/quit") return 0;
                        else break; 
                    }
                }
            }
        }
    }
    return 0;
} 

// helper method
void verbatim(const char *prefix, const char *data, int len, const char *suffix) {
  printf("%s", prefix);
  for (int i=0; i<len; i++) {
    if (data[i] == '\n')
      printf("<LF>");
    else if (data[i] == '\r') 
      printf("<CR>");
    else if (isprint(data[i])) 
      printf("%c", data[i]);
    else 
      printf("<0x%02X>", (unsigned int)(unsigned char)data[i]);
  }
  printf("%s", suffix);
}
