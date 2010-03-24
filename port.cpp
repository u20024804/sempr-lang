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

} //namespace cerl
