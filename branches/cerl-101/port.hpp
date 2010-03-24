#ifndef PORT_INCLUDED
#define PORT_INCLUDED

#include <sys/socket.h>
#include <netinet/in.h>

namespace cerl
{

    void set_limits(unsigned maxfd);
    void set_noblock(int fd);

} //namespace cerl

#endif // PORT_INCLUDED
