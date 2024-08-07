#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "sock.h"


void report(const char* msg, int terminate) {
    perror(msg);
    if (terminate) exit(-1); /* failure */
}

int main() {

    int socketFileDescriptor =  socket( AF_INET, /* versus AF_LOCAL */
                                        SOCK_STREAM, /* reliable, bidirectional */
                                        0); /* system picks protocol (TCP) */

    if (socketFileDescriptor < 0) report("socket", 1); /* terminate */

    /* get the address of the host */
    struct hostent* hostPointer = gethostbyname(Host); /* localhost: 127.0.0.1 */
    if (!hostPointer) report("gethostbyname", 1); /* is hostPointer NULL? */
    if (hostPointer->h_addrtype != AF_INET) /* versus AF_LOCAL */
        report("bad address family", 1);

    /* connect to the server: configure server's address 1st */
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = ((struct in_addr*) hostPointer->h_addr_list[0])->s_addr;
    serverAddress.sin_port = htons(PortNumber); /* port number in big-endian */
    if (connect(socketFileDescriptor, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0)
        report("connect", 1);

    /* Write some stuff and read the echoes. */
    puts("Connect to server, about to receive data...");

    while (1) {


        float correnteBateria;
        float tensaoBateria;
        float socEstimado;
        float latitude;
        float longitude;
        float altitude;
        
        //Client reads data
        char buffer[BuffSize + 1];
        memset(buffer, '\0', sizeof(buffer));
        int count = read(socketFileDescriptor, buffer, sizeof(buffer));
        if (count) {

            //Data will come in this format
            //tensaoBateria=value&correnteBateria=value&socEstimado=value&latitude=value&longitude=value&altitude=value

            char *token = strtok(buffer, "&");
            char *tensaoBateriaToken = strtok(token, "=");
            tensaoBateriaToken = strtok(NULL, "=");

            //Print value
            printf("tensaoBateria: %s\n", tensaoBateriaToken);
        }

    }

    puts("Client done, about to exit...");
    close(socketFileDescriptor); /* close the connection */
    return 0;
}