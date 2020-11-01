#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in in;
    bzero(&in, sizeof(in));
    
    in.sin_family = AF_INET;
    in.sin_port = htons(60000);
    inet_pton(AF_INET, "192.168.199.153", &in.sin_addr.s_addr);
    
    int cFd = connect(fd, (struct sockaddr*)&in, sizeof(in));
    if (cFd < 0)
    {
        printf("connect error!\n");
        return 0;
    }
    
    char buf1[1024] = "Hello service!";
    
    send(fd, buf1, strlen(buf1), 0);
    
    char buf[1024] = {0};
    recv(fd, buf, sizeof(buf), 0);
    printf("recv msg: %s\n", buf);
    
    return 0;
}