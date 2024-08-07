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
    puts("Connect to server, about to write some stuff...");

    while (1) {

        //if (connect(socketFileDescriptor, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0)
        //    report("connect", 1);



        //Randomize data variables
        srand(time(NULL));
        float correnteBateria = (float)rand() / (float)(RAND_MAX / 100 );
        float tensaoBateria = (float)rand() / (float)(RAND_MAX / 100 );
        float socEstimado = (float)rand() / (float)(RAND_MAX / 100 );
        float latitude = (float)rand() / (float)(RAND_MAX / 100 );
        float longitude = (float)rand() / (float)(RAND_MAX / 100 );
        float altitude = (float)rand() / (float)(RAND_MAX / 100 );
        
        char fmtBuf[256];
        memset(fmtBuf, '\0', sizeof(fmtBuf));
        sprintf(fmtBuf, "correnteBateria=%.2f&tensaoBateria=%.2f&socEstimado=%.2f&latitude=%.6f&longitude=%.6f&altitude=%.3f",
                correnteBateria, 
                tensaoBateria, 
                socEstimado, 
                latitude, 
                longitude, 
                altitude);

        if (write(socketFileDescriptor, fmtBuf, strlen(fmtBuf))) {
            char receiveBuffer[BuffSize + 1];
            memset(receiveBuffer, '\0', sizeof(receiveBuffer));
            if (read(socketFileDescriptor, receiveBuffer, sizeof(receiveBuffer))) {
                puts(receiveBuffer);
            }
        }
        sleep(3);
    }

    puts("Client done, about to exit...");
    close(socketFileDescriptor); /* close the connection */
    return 0;
}