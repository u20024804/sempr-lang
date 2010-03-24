#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cerl.hpp"

using namespace cerl;

tasklet_service *pservice;

class echo : public tasklet
{
public:
    static const char * const response;
    echo(int listenfd, int fd) :
            _listenfd(listenfd),
            _conn(fd)
    {
        //printf("new echo(%d): %x\n", _conn, this);
    }
    void run()
    {
        char buffer[128];
        message msg = recv(_conn, buffer, sizeof(buffer));
        if (*(int *)"exit" == *(int *)buffer)
        {
            close(_listenfd);
            pservice->stop();
        }
        int size = msg.content.ivalue;
        //printf("recv: %d, %s\n", size, buffer);
        msg = send(_conn, response, strlen(response));
        size = msg.content.ivalue;
        //printf("send: %d, %s\n", size, response);
    }
    ~echo()
    {
        //printf("~echo(%d): %x\n", _conn, this);
        close(_conn);
    }
private:
    int _listenfd;
    int _conn;
};

const char * const echo::response = "HTTP/1.1 200\r\nContent-Length: 14\r\n\r\nHello world!\r\n";

class server : public thread
{
public:
    server(int listenfd):
            _listenfd(listenfd)
    {
    }
    void run()
    {
        while (true)
        {
            int conn = accept(_listenfd);
            if (conn == -1)
            {
                printf("invalid conn socket!\n");
                break;
            }
            echo *client = new echo(_listenfd, conn);
            pservice->add(client);
        }
    }
private:
    int _listenfd;
};

#define PROCESSES 4

class manager : public thread
{
public:
    void run()
    {
        sockaddr_in localaddr = {0};
        localaddr.sin_family = AF_INET;
        localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localaddr.sin_port = htons(8889);
        int listenfd = port(&localaddr, 1);
        int conn = accept(listenfd);
        close(conn);

        char buffer[] = "exit";
        for (int i = 0; i < PROCESSES; i++)
        {
            struct sockaddr_in servaddr;
            memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            servaddr.sin_port = htons(8888);
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if(sock == -1)
            {
                --i;
                printf("socket fail!\n");
                continue;
            }
            int ret = connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr));
            if(ret == -1)
            {
                close(sock);
                --i;
                printf("connect fail!\n");
            }
            send(sock, buffer, sizeof(buffer), 0);
            close(sock);
            printf("exit success!\n");
        }
        delete this;
    }
};

int main()
{
    sockaddr_in localaddr = {0};
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localaddr.sin_port = htons(8888);
    int listenfd = port(&localaddr, 1);
    printf("start to listen...\n");

    bool father  = true;
    for (int i = 0; i < PROCESSES - 1; i++)
    {
        if (fork() == 0)
        {
            //close(listenfd);
            //return 0;
            father = false;
            break;
        }
    }
    if (father)
    {
        printf("in father\n");
        manager *m = new manager();
        m->start();
    }
    else
    {
        printf("in child\n");
    }
    pservice = new tasklet_service(4);
    pservice->start();

    server server0 = server(listenfd);
    server server1 = server(listenfd);
    server server2 = server(listenfd);
    server server3 = server(listenfd);
    server0.start();
    server1.start();
    server2.start();
    server3.start();
    server0.join();
    server1.join();
    server2.join();
    server3.join();

    printf("close...\n");
    delete pservice;
}
