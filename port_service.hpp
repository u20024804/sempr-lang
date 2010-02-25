#ifndef PORT_SERVICE_HPP_INCLUDED
#define PORT_SERVICE_HPP_INCLUDED

#include <sys/epoll.h>

#include <set>
#include <map>

#include "constant.hpp"
#include "common.hpp"
#include "lock.hpp"
#include "thread.hpp"

namespace cerl
{

    using std::set;
    using std::map;

    class tasklet;
    class tasklet_service;

    class port_service : public thread
    {
    public:
        port_service(tasklet_service& tasklet_service_, int maxevents=DEFAULT_MAXEVENTS);
        void run();
        void loop();
        virtual ~port_service();

        bool add_read(tasklet &tasklet_, int fd);
        bool add_write(tasklet &tasklet_, int fd);
        void del(tasklet &tasklet_, int fd);
        void on_finish(tasklet &tasklet_, int fd, finish_type type_);
        void shutdown(tasklet &tasklet_, int fd);

        bool stopped()
        {
            return _stop;
        }
        void stop(bool stop_=true)
        {
            if(stop_)
            {
                interrupt();
            }
            _stop = stop_;
        }
        void interrupt();
    protected:
        bool reset();
    private:
        tasklet_service& _tasklet_service;
        int _epollfd;
        int _interrupter;
        int _maxevents;
        epoll_event  *_epoll_events;
//        set<int> _read_fds;
//        set<int> _write_fds;
//        map<int, tasklet*> _fd_tasklet;
        spin _spin;
        bool _stop;
    };

} //namespace cerl

#endif // PORT_SERVICE_HPP_INCLUDED
