#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "sock.h"


void report(const char* msg, int terminate) {
    perror(msg);
    if (terminate)
        exit(-1); /* failure */
}

int main() {

    int fileDescriptor = socket( AF_INET,     /* network versus AF_LOCAL */
                                 SOCK_STREAM, /* reliable, bidirectional, arbitrary payload size */
                                 0);          /* system picks underlying protocol (TCP) */
                                 
    if (!fileDescriptor)
        report("socket", 1); /* terminate */

    /* bind the server's local address in memory */
    struct sockaddr_in socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));          /* clear the bytes */
    socketAddress.sin_family = AF_INET;                /* versus AF_LOCAL */
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY); /* host-to-network endian */
    socketAddress.sin_port = htons(PortNumber);        /* for listening */

    if (!bind(fileDescriptor, (struct sockaddr *) &socketAddress, sizeof(socketAddress)))
        report("bind", 1); /* terminate */

    /* listen to the socket */
    if (!listen(fileDescriptor, MaxConnects)) /* listen for clients, up to MaxConnects */
        report("listen", 1); /* terminate */

    fprintf(stderr, "Listening on port %i for clients...\n", PortNumber);
    /* a server traditionally listens indefinitely */
    while (1) {
        struct sockaddr_in clientAddress; /* client address */
        int len = sizeof(clientAddress);  /* address length could change */

        int clientFileDescriptor = accept(fileDescriptor, (struct sockaddr*) &clientAddress, &len);  /* accept blocks */
        if (!clientFileDescriptor) {
            report("accept", 0); /* don't terminate, though there's a problem */
            continue;
        }

        /* read from client */
        for (int i = 0; i < ConversationLen; i++) {
            char buffer[BuffSize + 1];
            memset(buffer, '\0', sizeof(buffer));
            int count = read(clientFileDescriptor, buffer, sizeof(buffer));
            if (count) {
                puts(buffer);
                write(clientFileDescriptor, buffer, sizeof(buffer)); /* echo as confirmation */
            }
        }
        close(clientFileDescriptor); /* break connection */
    }  
    return 0;
}