#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

int g_eFd = 0;

typedef struct TActor {
    int fd;
    int opt;
    void (*act_func)(struct TActor*);
}TActor;

void fd_act(TActor* act)
{
    int fd = act->fd;
    int opt = act->opt;
    
    printf("act  fd : %d\n", fd);
    
    if (opt | EPOLLIN)
    {
        char buf[1024] = {0};
        recv(fd, buf, sizeof(buf), 0);
        printf("recv msg: %s\n", buf);
    }
    
    if (opt | EPOLLOUT)
    {
        char buf[1024] = {"it is recv success"};
        send(fd, buf, sizeof(buf), 0);
        printf("send msg...\n");
    }
    
    
    
    if (opt | EPOLLHUP)
    {
        printf("shutdown fd\n");
        shutdown(fd, SHUT_RDWR);
        
        epoll_ctl(g_eFd, EPOLL_CTL_DEL, fd, NULL);
        free(act);
    }   
}

void add_fd(int fd)
{
    TActor* act = (TActor*)malloc(sizeof(TActor));
    act->fd = fd;
    act->opt = EPOLLIN | EPOLLOUT | EPOLLHUP; 
    act->act_func = fd_act;
    
    struct epoll_event ent;
    ent.events = act->opt;
    ent.data.ptr = act;
    
    epoll_ctl(g_eFd, EPOLL_CTL_ADD, fd, &ent);
}


void connect_act(TActor* act)
{
    int fd = act->fd;
    
    struct sockaddr_in in;
    int len = sizeof(in);
    int cliFd = accept(fd, (struct sockaddr*)&in, &len);
    if(cliFd < 0)
    {
        printf("accept error!\n");
        return;
    }
    
    printf("accept success %d \n", cliFd);
    add_fd(cliFd);
}

void add_connectFd(int fd)
{
    TActor* act = (TActor*)malloc(sizeof(TActor));
    act->fd = fd;
    act->opt = EPOLLIN; 
    act->act_func = connect_act;
    
    struct epoll_event ent;
    ent.events = act->opt;
    ent.data.ptr = act;
    
    epoll_ctl(g_eFd, EPOLL_CTL_ADD, fd, &ent);
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = htons(60000);
    inet_pton(AF_INET, "192.168.199.153", &in.sin_addr.s_addr);
    int res = 0;
    if (res = bind(fd, (struct sockaddr*)&in, sizeof(in)) < 0)
    {
        printf("bind error %d!\n", res);
        return 0;
    }
    
    if (listen(fd, 255) < 0)
    {
        printf("listen error!\n");
        return 0;
    }
    
    g_eFd = epoll_create(255);
    
    struct epoll_event ents[10] = {0};
    
    add_connectFd(fd);
    while(1)
    {
        int res = epoll_wait(g_eFd, ents, 10, 0);
        if (res < 0)
        {
            printf("res error!\n");
            break;
        }
        else if(res > 0)
        {
            printf("res : %d\n", res);
            
            for (int index = 0; index < res; ++index)
            {
                TActor* act = (TActor*)ents[index].data.ptr;
                act->act_func(act);
            }
            
        }
    }

    return 0;
}
