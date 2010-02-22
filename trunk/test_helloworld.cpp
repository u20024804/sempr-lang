#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "cerl.hpp"

using namespace cerl;

tasklet_service service(4);

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
        if(*(int *)"exit" == *(int *)buffer)
        {
            close(_listenfd);
            service.stop();
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

int main()
{
    service.start();
    sockaddr_in localaddr = {0};
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localaddr.sin_port = htons(8888);
    int listenfd = port(&localaddr, 1);
    printf("start to listen...\n");
    while (true)
    {
        int conn = accept(listenfd);
        if(conn == -1)
        {
            printf("invalid conn socket!\n");
            break;
        }
        echo *client = new echo(listenfd, conn);
        service.add(client);
    }
    printf("close...\n");
}
