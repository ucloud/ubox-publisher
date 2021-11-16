#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SERVER_SOCKET "#video3"

int main(int argc, char** argv) {
    int fd;
    struct sockaddr_un srv_addr;
    socklen_t srv_addr_len;
    struct sockaddr_un from_addr;
    socklen_t from_len = sizeof(from_addr);

    // init server socket
    if ((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("socket error\n");
        return -1;
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    memcpy(srv_addr.sun_path, SERVER_SOCKET, sizeof(SERVER_SOCKET)-1);
    srv_addr.sun_path[0] = 0;
    srv_addr_len = sizeof(srv_addr.sun_family)+strlen(SERVER_SOCKET );;

    if (bind(fd, (struct sockaddr *)&srv_addr, srv_addr_len) < 0) {
        perror("bind error\n");
        close(fd);
        return -1;
    }

    // read data
    int index = 0;
    int data_len;
    char buff[8192];
    char hexbuf[32768];
    bool run = true;
    while (run) {
        data_len = recvfrom(fd, buff, sizeof(buff), 0, (struct sockaddr *)&from_addr, &from_len);
        if (data_len < 0) {
            run = false;
            break;
        }

        int i=0;
        for(; i<data_len; i++) {
            snprintf(hexbuf+3*i, 4, "%02x ", buff[i]);
        }
        hexbuf[3*i] = 0;
        printf("%d recv from %s: size(%d) content:\n%s\n", index++, from_addr.sun_path+1, data_len, hexbuf);
        //snprintf(buff, 8192, "recv message %d", index);
        //send_ret = sendto(fd, buff, strlen(buff)+1, 0, (struct sockaddr *)&from_addr, from_len);
        //if (send_ret < 0) {
        //    perror("sendto error\n");
        //    break;
        //}
    }

    close(fd);
    return 0;
}