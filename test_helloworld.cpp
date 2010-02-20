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
    echo(int fd) : tasklet(),
            _conn(fd)
    {
        //printf("new echo(%d): %x\n", _conn, this);
    }
    void run()
    {
        char buffer[128];
        message msg = recv(_conn, buffer, sizeof(buffer));
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
        echo *client = new echo(conn);
        service.add(client);
    }
    close(listenfd);
    printf("close...\n");
}
