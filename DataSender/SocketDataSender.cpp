#include "SocketDataSender.hpp"
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

#define PortNumber 5555
#define MaxConnects 5
#define BuffSize 256
#define ConversationLen 3
#define Host "127.0.0.1"

//Socket client connection and implement the data sender interface

void SocketDataSender::sendData(const std::string& tag, const float& data) {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[BuffSize];
    char* message = (char*)malloc(sizeof(char) * 256);
    char* dataStr = (char*)malloc(sizeof(char) * 256);
    char* tagStr = (char*)malloc(sizeof(char) * 256);

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PortNumber);
    servaddr.sin_addr.s_addr = inet_addr(Host);

    // Convert float to string
    sprintf(dataStr, "%f", data);
    sprintf(tagStr, "%s", tag.c_str());

    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        printf("connection with the server failed");
        exit(EXIT_FAILURE);
    }

    // Send data to server
    sprintf(message, "%s,%s", tagStr, dataStr);
    send(sockfd, message, strlen(message), 0);
    printf("Message sent to server.\n");

    // Close the socket
    close(sockfd);
}

