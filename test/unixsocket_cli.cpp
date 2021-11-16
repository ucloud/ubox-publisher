#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>

#define CLIENT_SOCKET "@/tmp/publish_test_client"

int main(int argc, char** argv) {
    signed char ch;
    int cmd = 0;
    //while ((ch = getopt(argc, argv, "c:")) != -1) {
    //    switch (ch) {
    //        case 'c':
    //            cmd = atoi();
    //            break;
    //        default:
    //            std::cout << "unknown option " << ch << std::endl;
    //            showUsage();
    //    }
    //}

    int fd;
    struct sockaddr_un cli_addr;
    socklen_t cli_addr_len;
    struct sockaddr_un srv_addr;
    socklen_t srv_addr_len;

    // init client socket
    if ((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("socket error\n");
        return -1;
    }

    memset(&cli_addr, 0, sizeof(cli_addr));
    cli_addr.sun_family = AF_UNIX;
    memcpy(cli_addr.sun_path, CLIENT_SOCKET, strlen(CLIENT_SOCKET));
    cli_addr.sun_path[0] = 0;
    cli_addr_len = sizeof(cli_addr.sun_family) + strlen(CLIENT_SOCKET);

    if (bind(fd, (const struct sockaddr *)&cli_addr, cli_addr_len) < 0) {
        perror("bind error\n");
        close(fd);
        return -1;
    }

    // init server addr
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    memcpy(srv_addr.sun_path+1, argv[1],strlen(argv[1]));
    srv_addr_len = sizeof(srv_addr.sun_family) + strlen(argv[1]) + 1;
    srv_addr.sun_path[0] = 0;

    // run
    char buff[8192];
    buff[0] =0x20;
    buff[1] =0x21;
    buff[2] =0x09;
    buff[3] =0x01;
    int len;

    while(fgets(buff+8, 8192, stdin) != NULL) {
       if(buff[8] == 'q')
           break;
        buff[0] =0x20;
        buff[1] =0x21;
        buff[2] =0x09;
        buff[3] =0x01;
       int stringSize = strlen(buff+8);
       *(int*)(buff+4) = ntohl(stringSize);
        int ret = sendto(fd, buff, stringSize+8, 0, (struct sockaddr *)&srv_addr, srv_addr_len);
        if (ret == -1) {
            fprintf(stderr, "send error %s, %d, errno(%d)\n", srv_addr.sun_path+1, srv_addr_len, errno);
            break;
       }
       if ((len = recvfrom(fd, buff, 8192, 0, NULL, NULL)) < 0) {
           perror("recv error\n");
           break;
       }
       buff[len] = 0;
       if ( buff[0] != 0x01 ||
            buff[1] != 0x09 ||
            buff[2] != 0x21 ||
            buff[3] != 0x20
          ) {
           printf ("receive unkown data, lenth:%d, content %02x %02x %02x %02x %02x %02x %02x %02x \n",
                   len, buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7]);
       } else {
           printf("receive data, lenth:%d, data size:%d, data content:\n%s\n", len, htonl(*(int*)(buff+4)), buff+8);
       }
    }

    close(fd);
    return 0;
}
