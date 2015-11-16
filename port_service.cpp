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
        ev.events = EPOLLIN | EPOLLERR | EPOLLET;
        ev.data.ptr = NULL;
        epoll_ctl(_epollfd, EPOLL_CTL_ADD, _interrupter, &ev);
    }

    bool port_service::add_read(netfile &netfile_)
    {
        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
        ev.data.ptr = &netfile_;
        int result;
        if(netfile_._net_event & net_write)
        {
            ev.events |= EPOLLOUT;
            result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
        }
        else
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev);
        }
        if (result != 0)
        {
            assert(errno != ENOENT);
            return false;
        }
        netfile_._net_event |= net_read;
        return true;
    }

    bool port_service::add_write(netfile &netfile_)
    {
        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
        ev.data.ptr = &netfile_;
        int result;
        if(netfile_._fd != -1)
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
        }
        else
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev);
            netfile_._fd = fd;
        }
        if (result != 0)
        {
            assert(errno != ENOENT);
            return false;
        }
        return true;
    }

    void port_service::on_finish(tasklet &tasklet_)
    {
        del(tasklet_);
        return;
    }

    void port_service::del(tasklet &tasklet_)
    {
        epoll_event ev = { 0, { 0 } };
        epoll_ctl(_epollfd, EPOLL_CTL_DEL, tasklet_._fd, &ev);
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
            tasklet *_ptasklet = (tasklet *)_epoll_events[i].data.ptr;
            if (_ptasklet == NULL)
            {
                reset();
                continue;
            }
            tasklet& target = *_ptasklet;
            int fd = target._fd;
            uint32_t events = _epoll_events[i].events;
            char *buffer = target._buffer;
            int buffer_size = target._buffer_size;
            int flags = target._flags;
            if (events & EPOLLERR)
            {
                _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
            }
            else if (events & EPOLLHUP)
            {
                _tasklet_service._channel_manager.dispatch(target._channel, msg_hup);
            }
            else if ((events & EPOLLIN))
            {
                if(target._net_state == listening)
                {
                    const message msg_accept = {{fd}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_accept);
                    continue;
                }
                int len = ::recv(fd, buffer, buffer_size, flags);
                if (len < 0)
                {
                    if (errno == EAGAIN)
                    {
                        continue;
                    }
                    else
                    {
                        _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
                    }
                }
                else if(len == 0)
                {
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_closed);
                    continue;
                }
                else if(len <= buffer_size)
                {
                    target._finished_buffer += len;
                    message msg_ok = {{target._finished_buffer}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_ok);
                }
                else // len > buffer_size
                {
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
                }
            }
            else if (events & EPOLLOUT)
            {
                if(target._net_state == connectting)
                {
                    const message msg_connect = {{fd}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_connect);
                    continue;
                }

                int len = ::send(fd, buffer, buffer_size, flags);
                if (len < 0)
                {
                    if (errno == EAGAIN)
                    {
                        continue;
                    }
                    else
                    {
                        _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
                    }
                }
                else if(len == 0)
                {
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_closed);
                    continue;
                }
                else if(len == buffer_size)
                {
                    target._finished_buffer += len;
                    message msg_ok = {{target._finished_buffer}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_ok);
                }
                else if(len < buffer_size)
                {
                    target._finished_buffer += len;
                    target._buffer += len;
                    target._buffer_size -= len;
                    continue;
                }
                 else // len > buffer_size
                {
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
                }
           }
            else
            {
                throw exception(__FILE__, __LINE__);
            }
        }
    }

} //namespace cerl
