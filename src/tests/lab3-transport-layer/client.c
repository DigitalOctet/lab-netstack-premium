/**
 * @file client.c
 * 
 * @brief (Checkpoint 7 & 8）Establish TCP connection between ns1 and ns4, 
 * where ns1 is the client and ns4 is the server. The client sends a message 
 * to the server. The server checks whether the message is correct and sends it
 * back the client. The client checks it again.
 * 
 * Virtual network:
 *      ns1 -- ns2 -- ns3 -- ns4
 */

#include "util.h"
#include <tcp/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    struct addrinfo hints, *listp, *p;

    if(argc != 3){
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; /* Open a connection */
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host, port, &hints, &listp) < 0){
        perror("getaddrinfo() error");
        exit(0);
    }

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Connect to the server */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break;                                         /* Success */
        close(clientfd); /* Connect failed, try another */
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p){ /* All connects failed */
        perror("All connects failed");
        exit(0);
    }
    /* The last connect succeeded */

    memset(buf, 0, sizeof(buf));
    rio_writen(clientfd, message, strlen(message));
    rio_readn(clientfd, buf, strlen(message));
    if(strcmp(message, buf)){
        fprintf(stderr, "Message is damaged!\n");
        exit(0);
    }
    
    return 0;
}