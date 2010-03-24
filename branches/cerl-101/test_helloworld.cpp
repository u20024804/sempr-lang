#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "cerl.hpp"

using namespace cerl;

atomic_long sum;

tasklet_service service(4);

class echo : public tasklet
{
public:
    static const char * const response;
    echo(int listenfd, int fd) :
            _listenfd(listenfd),
            _conn(fd)
    {
        sum++;
        //printf("new echo(%d): %x\n", _conn, this);
    }
    void run()
    {
        char buffer[128];
        message msg;
        try
        {
            msg = recv(_conn, buffer, sizeof(buffer));
        }
        catch (close_exception)
        {
            printf("recv exception\n");
            throw;
        }
        if (*(int *)"exit" == *(int *)buffer)
        {
            printf("exit\n");
            printf("sum: %ld\n", sum.get()-1);

            service.stop();
            close();
            close(_listenfd);
            _conn = -1;
            return;
        }
        int size = msg.content.ivalue;
        //printf("recv: %d, %s\n", size, buffer);
        try
        {
            msg = send(_conn, response, strlen(response));
        }
        catch (close_exception)
        {
            printf("send exception\n");
            throw;
        }
        size = msg.content.ivalue;
        //printf("send: %d, %s\n", size, response);
    }
    ~echo()
    {
        //printf("~echo(%d): %x\n", _conn, this);
        if (_conn != -1)
        {
            close();
        }
    }
private:
    int _listenfd;
    int _conn;
};

const char * const echo::response = "HTTP/1.1 200\r\nContent-Length: 14\r\n\r\nHello world!\r\n";

class server : public tasklet
{
public:
    void run()
    {
        sockaddr_in localaddr = {0};
        localaddr.sin_family = AF_INET;
        localaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
        localaddr.sin_port = htons(8888);
        int listenfd = port(&localaddr, 1);
        printf("start to listen...\n");

        while (true)
        {
            int conn = accept();
            if (conn == -1)
            {
                printf("invalid conn socket!\n");
                return;
            }
            echo *client = new echo(listenfd, conn);
            service.add(client);
        }
    }
    ~server()
    {
        printf("close...\n");
    }
private:
};

int main()
{
    server *http = new server();
    service.add(http);
    service.start();
}
