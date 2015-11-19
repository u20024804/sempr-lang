#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
#include <asm/unistd.h>
#else // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
#include <sys/eventfd.h>
#endif // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8

#include <sys/socket.h>
#include <sys/epoll.h>

#include "throwable.hpp"
#include "port_service.hpp"
#include "tasklet_service.hpp"
#include "channel.hpp"

namespace cerl
{

    port_service::port_service(tasklet_service& tasklet_service_, int maxevents) :
            _tasklet_service(tasklet_service_),
            _maxevents(maxevents+1),
            _epoll_events(new epoll_event[maxevents+1]),
            _stop(false)
    {
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
        _interrupter = syscall(__NR_eventfd, 0);
#else // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
        _interrupter = ::eventfd(0, 0);
#endif // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8

        ::fcntl(_interrupter, F_SETFL, O_NONBLOCK);

        _epollfd = epoll_create(_maxevents);

        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLIN | EPOLLERR;
        ev.data.ptr = NULL;
        epoll_ctl(_epollfd, EPOLL_CTL_ADD, _interrupter, &ev);
    }

    bool port_service::add_read(netfile &netfile_)
    {
        if(netfile_._net_event & net_read)
        {
            return true;
        }

        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        ev.data.ptr = &netfile_;
        int result;
        if(netfile_._net_event & net_write)
        {
            ev.events |= EPOLLOUT;
            result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, netfile_._fd, &ev);
        }
        else
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, netfile_._fd, &ev);
        }
        if (result != 0)
        {
            assert(errno != ENOENT);
            return false;
        }
        netfile_._net_event |= net_read;
        return true;
    }

    bool port_service::del_read(netfile &netfile_)
    {
        if(netfile_._net_event & net_read == 0)
        {
            return true;
        }

        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLERR | EPOLLHUP;
        ev.data.ptr = &netfile_;
        int result;
         if(netfile_._net_event & net_write)
        {
            ev.events |= EPOLLOUT;
            result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, netfile_._fd, &ev);
        }
        else
        {
            epoll_event ev = { 0, { 0 } };
            result = epoll_ctl(_epollfd, EPOLL_CTL_DEL, netfile_._fd, &ev);
        }
        if (result != 0)
        {
            assert(errno != ENOENT);
            return false;
        }
        netfile_._net_event &= ~net_read;
        return true;
     }

    bool port_service::add_write(netfile &netfile_)
    {
        if(netfile_._net_event & net_write)
        {
            return true;
        }

        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
        ev.data.ptr = &netfile_;
        int result;
        if(netfile_._net_event & net_read)
        {
            ev.events |= EPOLLIN;
            result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, netfile_._fd, &ev);
        }
        else
        {
            epoll_event ev = { 0, { 0 } };
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, netfile_._fd, &ev);
        }
        if (result != 0)
        {
            assert(errno != ENOENT);
            return false;
        }
        netfile_._net_event |= net_write;
        return true;
    }

    bool port_service::del_write(netfile &netfile_)
    {
        if(netfile_._net_event & net_write == 0)
        {
            return true;
        }

        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLERR | EPOLLHUP;
        ev.data.ptr = &netfile_;
        int result;
        if(netfile_._net_event & net_read)
        {
            ev.events |= EPOLLIN;
            result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, netfile_._fd, &ev);
        }
        else
        {
            epoll_event ev = { 0, { 0 } };
            result = epoll_ctl(_epollfd, EPOLL_CTL_DEL, netfile_._fd, &ev);
        }
        if (result != 0)
        {
            assert(errno != ENOENT);
            return false;
        }
        netfile_._net_event &= ~net_write;
        return true;
      }

    void port_service::on_finish(netfile &netfile_)
    {
        del(netfile_);
        return;
    }

    void port_service::del(netfile &netfile_)
    {
        epoll_event ev = { 0, { 0 } };
        epoll_ctl(_epollfd, EPOLL_CTL_DEL, netfile_._fd, &ev);
    }

    port_service::~port_service()
    {
        delete[] _epoll_events;
        close(_epollfd);
        close(_interrupter);
    }

    void port_service::run()
    {
        while (!_stop)
        {
            loop();
        }
    }

    void port_service::interrupt()
    {
        uint64_t counter(1UL);
        int result = ::write(_interrupter, &counter, sizeof(uint64_t));
        if(result == -1)
        {
            throw exception(__FILE__, __LINE__);
        }
    }

    bool port_service::reset()
    {
        uint64_t counter(0);
        int bytes_read = ::read(_interrupter, &counter, sizeof(uint64_t));
        bool was_interrupted = (bytes_read > 0);
        return was_interrupted;
    }

    static const message msg_ok = {{0}, port_msg};
    static const message msg_error = {{-1}, port_msg};
    static const message msg_hup = {{-2}, port_msg};
    static const message msg_not_found = {{-3}, port_msg};
    static const message msg_closed = {{-4}, port_msg};

    void port_service::loop()
    {
        int count = epoll_wait(_epollfd, _epoll_events, _maxevents, -1);
        if (count == -1)
        {
            if (errno == EINTR)
            {
                return;
            }
            else
            {
                throw exception(__FILE__, __LINE__);
            }
        }
        if (count == 0)
        {
            throw exception(__FILE__, __LINE__);
        }
        mutex_lock lock(_tasklet_service._mutex);
        for (int i = 0; i < count; i++)
        {
            netfile *netfile_ = (netfile *)_epoll_events[i].data.ptr;
            if (netfile_ == NULL)
            {
                reset();
                continue;
            }
            netfile& target = *netfile_;
            int fd = target._fd;
            uint32_t events = _epoll_events[i].events;
            if (events & EPOLLERR)
            {
                _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_error);
            }
            else if (events & EPOLLHUP)
            {
                _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_hup);
            }
            else if ((events & EPOLLIN))
            {
                if(target._net_state == listening)
                {
                    const message msg_accept = {{fd}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_accept);
                    continue;
                }
                const int to_read = target._to_read;
                int len = target.on_recv();
                if (len < 0)
                {
                    if (errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_error);
                    }
                }
                else if(len == 0)
                {
                    _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_closed);
                    continue;
                }
                else if(len >= to_read)
                {
                    _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_ok);
                }
                else
                {
                    continue;
                }
            }
            else if (events & EPOLLOUT)
            {
                if(target._net_state == connectting)
                {
                    const message msg_connect = {{fd}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_connect);
                    continue;
                }

                const int to_write = target._to_write;
                int len = target.on_send();
                if (len < 0)
                {
                    if (errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_error);
                    }
                }
                else if(len == 0)
                {
                    _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_closed);
                    continue;
                }
                else if(len >= to_write)
                {
                    _tasklet_service._channel_manager.dispatch(target._tasklet._channel, msg_ok);
                    del_write(target);
                }
                else
                {
                    continue;
                }
            }
            else
            {
                throw exception(__FILE__, __LINE__);
            }
        }
    }

} //namespace cerl
