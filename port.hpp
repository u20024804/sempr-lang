#ifndef PORT_INCLUDED
#define PORT_INCLUDED

#include <sys/socket.h>
#include <netinet/in.h>

namespace cerl
{

    void set_limits(unsigned maxfd);
    void set_noblock(int fd);
    int port(const sockaddr_in *addr, int backlog=128, int socket_type=SOCK_STREAM, int protocol=0);
    int accept(int socket);
    //int close(int fd);

} //namespace cerl

#endif // PORT_INCLUDED
