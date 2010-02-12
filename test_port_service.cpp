#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "cerl.hpp"

using namespace cerl;

tasklet_service service(4);

class echo : public tasklet
{
public:
    echo(int fd) : tasklet(),
            _conn(fd)
    {
        printf("new echo(%d): %x\n", _conn, this);
    }
    void run()
    {
        for (int i = 0; i < 10; i++)
        {
            char buffer[8] = "abcde";
            int size = recv(_conn, buffer, sizeof(buffer));
            printf("recv: %d, %s\n", size, buffer);
            size = send(_conn, buffer, size);
            printf("send: %d, %s\n", size, buffer);
        }
    }
    ~echo()
    {
        printf("~echo(%d): %x\n", _conn, this);
        close(_conn);
    }
private:
    int _conn;
};

int main()
{
    service.start();
    sockaddr_in localaddr = {0};
    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localaddr.sin_port = htons(8888);
    int listenfd = port(&localaddr, 1);
    while (true)
    {
        int conn = accept(listenfd);
        echo *client = new echo(conn);
        service.add(client);
    }
    close(listenfd);
}
