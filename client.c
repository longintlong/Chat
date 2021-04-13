#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>

#define	MAXLINE	 4096
#define SA struct sockaddr


int main(int argc, char **argv){
    int sockfd,ret;
    struct sockaddr_in serveaddr;
    char buff[MAXLINE];
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("%s\n", "sock error");
    }
    memset(&serveaddr, 0 , sizeof(serveaddr));
    serveaddr.sin_family = AF_INET;
    serveaddr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip, &serveaddr.sin_addr) <= 0){
        printf("%s\n", "IP error");
    }
    if(connect(sockfd, (SA *)&serveaddr, sizeof(serveaddr)) < 0){
        printf("%s\n", "connect error");
    }
    struct pollfd fds[2];
    int pipefd[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN|POLLHUP;
    fds[1].revents = 0;
    pipe(pipefd);
    while(1){
        ret = poll(fds, 2, -1);
        if(ret < 0){
            printf("%s\n","poll error!\n");
            break;
        }
        if(fds[0].revents & POLLIN){
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE);
            assert(ret != -1);
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE);
            assert(ret != -1);
        }
        if(fds[1].revents & POLLHUP){
            printf("server close\n");
            break;
        }
        if(fds[1].revents & POLLIN){
            memset(buff, 0, sizeof(buff));
            if(read(sockfd, buff, MAXLINE) < 0){
                printf("read error\n");
            }
            printf("%s\n",buff);
        }
    }
    exit(0);
}
