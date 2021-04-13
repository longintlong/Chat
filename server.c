#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <assert.h>

#define SA struct sockaddr
#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define	SERV_PORT 9877
#define FD_LIMIT 65535

struct client_data{
    struct sockaddr_in cliaddr;
    char *write_buff;
    char buff[BUFFER_SIZE];
};

int setnoblocking(int fd){
    int old = fcntl(fd, F_GETFL);
    int new = old | O_NONBLOCK;
    fcntl(fd, F_SETFL, new);
    return old;
}

int main(){
    int listenfd, connfd;
    pid_t child_pid;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    if((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        printf("%s\n","socket error");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenfd, (SA *)&servaddr, sizeof(servaddr)) == -1){
        printf("%d,%s\n", errno, "bind error");
        exit(1);
    }
    if ((listen(listenfd, 7)) == -1){
        printf("%d,%s\n", errno, "listen error");
        exit(1);
    }

    struct client_data users[FD_LIMIT];
    struct pollfd fds[USER_LIMIT + 1];
    char error[4096];
    int user_count = 0, ret, fd;
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;
    for (int i=1;i < USER_LIMIT + 1;i++){
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    for (int i=1;i < FD_LIMIT;i++){
        users[i].write_buff = 0;
    }
    while(1) {
        ret = poll(fds, user_count + 1, -1);
        if (ret < 0) {
            printf("poll error!\n");
            break;
        }
        for (int i = 0; i < user_count + 1; i++) {
            fd = fds[i].fd;
            if ((fd == listenfd) && (fds[i].revents & POLLIN)) {
                if ((connfd = accept(listenfd, (SA *) &cliaddr, &len)) < 0) {
                    printf("%s\n", "accept error");
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    printf("too much user\n");
                    close(connfd);
                    continue;
                }
                user_count++;
                users[connfd].cliaddr = cliaddr;
                setnoblocking(connfd);
                fds[user_count].revents = 0;
                fds[user_count].fd = connfd;
                fds[user_count].events = POLLIN | POLLERR | POLLRDHUP;
                printf("new user, now the user count is %d\n", user_count);
            } else if (fds[i].revents & POLLERR) {
                printf("error from %d\n", fds[i].fd);
                memset(error, 0, 4096);
                socklen_t len = sizeof(error);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                    printf("get sock option failed\n");
                }
                continue;
            } else if (fds[i].revents & POLLRDHUP) {
                printf("user %d %d quit chat\n", fd, users[fd].cliaddr.sin_addr.s_addr);
                users[fd] = users[fds[user_count].fd];
                fds[i] = fds[user_count];
                close(fd);
                i--;
                user_count--;
            } else if (fds[i].revents & POLLIN) {
                memset(users[fd].buff, 0, BUFFER_SIZE);
                ret = recv(fd, users[fd].buff, BUFFER_SIZE - 1, 0);
                if (ret <= 0) {
                    if(ret == 0)
                        printf("user %d quit chat\n", fd);
                    else
                        printf("user %d recv error\n", fd);
                    users[fd] = users[fds[user_count].fd];
                    fds[i] = fds[user_count];
                    close(fd);
                    i--;
                    user_count--;
                }
                else if (ret > 0) {
                    printf("get %d bytes data from %d\n", ret, fd);
                    for (int j = 1; j < user_count + 1; j++) {
                        if (fds[j].fd == fd) continue;
                        fds[j].events &= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buff = users[fd].buff;
                    }
                }
            }else if (fds[i].revents & POLLOUT) {
                if (!users[fd].write_buff) {
                    continue;
                }
                ret = send(fd, users[fd].write_buff, strlen(users[fd].write_buff), 0);
                users[fd].write_buff = 0;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    close(fds[0].fd);
}
