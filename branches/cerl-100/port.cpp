#include "port.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>

#include "throwable.hpp"

namespace cerl
{

    void set_limits(unsigned maxfd)
    {
        struct rlimit rlmt;

        if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1)
        {
            throw exception(__FILE__, __LINE__);
        }
        if (rlmt.rlim_cur >= maxfd)
            return;
        rlmt.rlim_cur = maxfd;
        if (rlmt.rlim_max < rlmt.rlim_cur)
            rlmt.rlim_max = maxfd;
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1)
        {
            throw exception(__FILE__, __LINE__);
        }
    }

    void set_noblock(int fd)
    {
        int flag, ret;

        flag = fcntl(fd, F_GETFL);
        if (flag == -1)
        {
            throw exception(__FILE__, __LINE__);
        }
        ret = fcntl(fd, F_SETFL, flag | O_NONBLOCK);
        if (ret == -1)
        {
            throw exception(__FILE__, __LINE__);
        }
    }

    int port(const struct sockaddr_in *addr, int backlog, int socket_type, int protocol)
    {
        int listenfd = socket(addr->sin_family, socket_type, protocol);
        if(listenfd == -1)
        {
            return -1;
        }
        int on = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
               (const void*)&on, sizeof(on));
        if(bind(listenfd, (struct sockaddr*)addr, sizeof(sockaddr_in)) == -1)
        {
            close(listenfd);
            return -1;
        }
        if(listen(listenfd, backlog) == -1)
        {
            close(listenfd);
            return -1;
        }
        return listenfd;
    }

    int accept(int socket)
    {
        int conn = -1;
        while(true)
        {
            conn = ::accept(socket, NULL, NULL);
            if(conn == -1)
            {
                if(errno == EINTR)
                {
                    continue;
                }
                else
                {
                    return -1;
                }
            }
            else
            {
                break;
            }
        }
        set_noblock(conn);
        return conn;
    }

} //namespace cerl
