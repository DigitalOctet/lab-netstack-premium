/**
 * @file server.c
 * 
 * @brief (Checkpoint 7 & 8）Establish TCP connection and send/receive data.
 */

#include "util.h"
#ifndef STANDARD
#include <tcp/socket.h>
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], *port;
    unsigned short client_port;
    struct addrinfo hints, *listp, *p;

    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    port = argv[1];

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_protocol = IPPROTO_TCP;
    getaddrinfo(NULL, port, &hints, &listp);

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;       /* Success */
        close(listenfd); /* Bind failed, try the next */
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p){ /* All connects failed */
        perror("all connects failed");
        exit(0);
    }

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0){
        close(listenfd);
        perror("listen() error");
        exit(0);
    }

    size_t n;
    char buf[MAXLINE];
    char *bufp = buf;
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        inet_ntop(AF_INET, &((struct sockaddr_in *)&clientaddr)->sin_addr, 
                  client_hostname, INET_ADDRSTRLEN);
        client_port = ntohs(((struct sockaddr_in *)&clientaddr)->sin_port);
        printf("connected to (%s %d)\n", client_hostname, client_port);
        while((n = rio_readn(connfd, bufp, 10)) != 0){
            printf("server received %d byte(s)\n", (int)n);
            rio_writen(connfd, bufp, n);
            bufp += n;
        }
        break;
    }
    close(listenfd);
    close(connfd);

    return 0;
}