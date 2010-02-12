#include <errno.h>

#include <fcntl.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
# include <asm/unistd.h>
#else // __GLIBC__ == 2 && __GLIBC_MINOR__ < 8
# include <sys/eventfd.h>
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
            _read_fds(),
            _write_fds(),
            _spin(),
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
        ev.data.fd = _interrupter;
        epoll_ctl(_epollfd, EPOLL_CTL_ADD, _interrupter, &ev);
    }

    bool port_service::add_read(tasklet &tasklet_, int fd)
    {
        spin_lock lock(_spin);
        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        if (_write_fds.find(fd) != _write_fds.end())
        {
            ev.events |= EPOLLOUT;
        }
        ev.data.fd = fd;
        int result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
        if (result != 0 && errno == ENOENT)
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev);
        }
        if (result != 0)
        {
            return false;
        }
        _read_fds.insert(fd);
        _fd_tasklet[fd] = &tasklet_;
        return true;
    }

    bool port_service::add_write(tasklet &tasklet_, int fd)
    {
        spin_lock lock(_spin);
        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
        if (_read_fds.find(fd) != _read_fds.end())
        {
            ev.events |= EPOLLIN;
        }
        ev.data.fd = fd;
        int result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
        if (result != 0 && errno == ENOENT)
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev);
        }
        if (result != 0)
        {
            return false;
        }
        _write_fds.insert(fd);
        _fd_tasklet[fd] = &tasklet_;
        return true;
    }

    void port_service::on_finish(tasklet &tasklet_, int fd, finish_type type_)
    {
        spin_lock lock(_spin);
        epoll_event ev = { 0, { 0 } };
        ev.events = EPOLLERR | EPOLLHUP;
        ev.data.fd = fd;
        if (type_ == finish_read)
        {
            _read_fds.erase(fd);
            if (_write_fds.find(fd) == _write_fds.end())
            {
                del(tasklet_, fd);
                return;
            }
            ev.events |= EPOLLOUT;
        }
        else if (type_ == finish_write)
        {
            _write_fds.erase(fd);
            if (_read_fds.find(fd) == _read_fds.end())
            {
                del(tasklet_, fd);
                return;
            }
            ev.events |= EPOLLIN;
        }
        else
        {
            throw exception();
        }
        int result = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev);
        if (result != 0 && errno == ENOENT)
        {
            result = epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev);
        }
        if (result != 0)
        {
            throw exception();
        }
    }

    void port_service::del(tasklet &tasklet_, int fd)
    {
        epoll_event ev = { 0, { 0 } };
        epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, &ev);
    }

    void port_service::shutdown(tasklet &tasklet_, int fd)
    {
        spin_lock lock(_spin);
        _read_fds.erase(fd);
        _write_fds.erase(fd);
        epoll_event ev = { 0, { 0 } };
        epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, &ev);
        _fd_tasklet.erase(fd);
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
            throw exception();
        }
    }

    bool port_service::reset()
    {
        uint64_t counter(0);
        int bytes_read = ::read(_interrupter, &counter, sizeof(uint64_t));
        bool was_interrupted = (bytes_read > 0);
        return was_interrupted;
    }

    static message msg_error = {{-1}, port_msg};
    static message msg_hup = {{-2}, port_msg};
    static message msg_not_found = {{-3}, port_msg};

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
                throw exception();
            }
        }
        if (count == 0)
        {
            throw exception();
        }
        mutex_lock lock(_tasklet_service._mutex);
        for (int i = 0; i < count; i++)
        {
            int fd = _epoll_events[i].data.fd;
            if (fd == _interrupter)
            {
                reset();
                continue;
            }
            uint32_t events = _epoll_events[i].events;
            map<int, tasklet *>::iterator it = _fd_tasklet.find(fd);
            if (it == _fd_tasklet.end())
            {
                throw exception();
            }
            tasklet &target = *it->second;
            char *buffer = target._buffer;
            size_t buffer_size = target._buffer_size;
            int flags = target._flags;
            if (events & EPOLLERR)
            {
                on_finish(target, fd, finish_read);
                on_finish(target, fd, finish_write);
                _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
            }
            else if (events & EPOLLHUP)
            {
                on_finish(target, fd, finish_read);
                on_finish(target, fd, finish_write);
                _tasklet_service._channel_manager.dispatch(target._channel, msg_hup);
            }
            else if ((events & EPOLLIN))
            {
                int len = ::recv(fd, buffer, buffer_size, flags);
                if (len == -1)
                {
                    if (errno == EAGAIN)
                    {
                        continue;
                    }
                    else
                    {
                        on_finish(target, fd, finish_read);
                        _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
                    }
                }
                else
                {
                    on_finish(target, fd, finish_read);
                    message msg_ok = {{len}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_ok);
                }
            }
            else if (events & EPOLLOUT)
            {
                int len = ::send(fd, buffer, buffer_size, flags);
                if (len == -1)
                {
                    if (errno == EAGAIN)
                    {
                        continue;
                    }
                    else
                    {
                        on_finish(target, fd, finish_write);
                        _tasklet_service._channel_manager.dispatch(target._channel, msg_error);
                    }
                }
                else
                {
                    on_finish(target, fd, finish_write);
                    message msg_ok = {{len}, port_msg};
                    _tasklet_service._channel_manager.dispatch(target._channel, msg_ok);
                }
            }
            else
            {
                throw exception();
            }
        }
    }

} //namespace cerl
