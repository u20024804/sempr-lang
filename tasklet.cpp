#include <iostream>
#include <sstream>
#include <ucontext.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "constant.hpp"
#include "common.hpp"
#include "tasklet.hpp"
#include "threadlet.hpp"
#include "tasklet_service.hpp"
#include "port.hpp"

namespace cerl
{

    size_t tasklet::default_stack_size = STACK_SIZE;

    using std::cerr;
    using std::hex;
    using std::endl;

    void tasklet::send(tasklet& target, const message& msg)
    {
        tasklet_lock lock(this);
        _ptasklet_service->send(target, *this, msg);
    }

    message tasklet::recv(double timeout)
    {
        tasklet_lock lock(this);
        return _ptasklet_service->recv(*this, timeout);
    }

    static const message msg_fail = {{-1}, port_msg};
    static const message msg_timeout = {{-1}, no_msg};

    message tasklet::send(int fd, const void *buf, size_t len, int flags)
    {
        tasklet_lock lock(this);
        on_port_finish on_port_finish_(this, fd, on_port_finish::op_read);
        _buffer = (char *)buf;
        _buffer_size = len;
        _flags = flags;
        bool success = _ptasklet_service->add_write(*this, fd);
        if (!success)
        {
            return msg_fail;
        }

        message msg = _ptasklet_service->recv(*this);
        if (msg.type != port_msg || msg.content.ivalue < 0)
        {
            if(msg.type == port_msg && msg.content.ivalue == -4)
            {
                throw close_exception();
            }
            std::stringstream output;
            output << "msg: {" << msg.type << ", " << msg.content.ivalue << "}" << endl;
            throw exception(__FILE__, __LINE__, output.str());
        }
        return msg;
    }

    message tasklet::recv(netfile netfile_, void *buf, size_t len, double timeout)
    {
        tasklet_lock lock(this);

        if(netfile_._recv_buff.readable() >= len)
        {
            netfile_._recv_buff.read((char *)buf, len);
            return message(buf, port_msg);
        }

        bool success = _ptasklet_service->add_read(*this, fd);
        if (!success)
        {
            return msg_fail;
        }

        message msg = _ptasklet_service->recv(*this, timeout);
        if(msg.type == no_msg)
        {
            if(msg.content.ivalue != 0 || timeout == infinity)
            {
                throw exception(__FILE__, __LINE__);
            }
            return msg_timeout;
        }
        else if (msg.type != port_msg || msg.content.ivalue < 0)
        {
            if(msg.type == port_msg && msg.content.ivalue == -4)
            {
                throw close_exception();
            }
            std::stringstream output;
            output << "msg: {" << msg.type << ", " << msg.content.ivalue << "}" << endl;
            throw exception(__FILE__, __LINE__, output.str());
        }
        return msg;
    }

    int tasklet::port(const struct sockaddr_in *addr, int backlog, int socket_type, int protocol)
    {
        int listenfd = socket(addr->sin_family, socket_type, protocol);
        if(listenfd == -1)
        {
            return -1;
        }
        set_noblock(listenfd);
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

        _net_state = listening;

        bool success = _ptasklet_service->add_read(*this, listenfd);
        if (!success)
        {
            return -1;
        }

        return listenfd;
    }

    int tasklet::accept(sockaddr * addr, socklen_t * addrlen)
    {
        tasklet_lock lock(this);

        message msg = _ptasklet_service->recv(*this);

        if (msg.type != port_msg || msg.content.ivalue < 0)
        {
            if(msg.type == port_msg && msg.content.ivalue == -4)
            {
                throw close_exception();
            }
            std::stringstream output;
            output << "msg: {" << msg.type << ", " << msg.content.ivalue << "}" << endl;
            throw exception(__FILE__, __LINE__, output.str());
        }
        return ::accept(msg.content.ivalue, addr, addrlen);
    }

    int tasklet::connect(const struct sockaddr *addr, double timeout)
    {
        int connectfd = -1;
        if ((connectfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        {
            return connectfd;
        }
        set_noblock(connectfd);

        int ret = ::connect(connectfd, addr, sizeof(addr));
        if(ret == 0)
        {
            return connectfd;
        }
        else if(ret != -1)
        {
            throw exception(__FILE__, __LINE__);
        }
        else if(errno != EINPROGRESS)
        {
            return ret;
        }

        tasklet_lock lock(this);

        _net_state = connectting;

        bool success = _ptasklet_service->add_write(*this, connectfd);
        if (!success)
        {
            return -1;
        }

        message msg = _ptasklet_service->recv(*this, timeout);
        if (msg.type != port_msg || msg.content.ivalue < 0)
        {
            if(msg.type == port_msg && msg.content.ivalue == -4)
            {
                throw close_exception();
            }
            std::stringstream output;
            output << "msg: {" << msg.type << ", " << msg.content.ivalue << "}" << endl;
            throw exception(__FILE__, __LINE__, output.str());
        }

        _net_state = serving;

        ret = ::connect(connectfd, addr, sizeof(addr));
        if(ret == 0)
        {
            set_noblock(connectfd);
        }
        else
        {
            connectfd = -1;
        }
        return connectfd;
     }

    void tasklet::close()
    {
        _ptasklet_service->close(*this);
    }

    void tasklet::close(int fd)
    {
        ::close(fd);
    }

    void tasklet::sleep(double timeout)
    {
        tasklet_lock lock(this);
        _ptasklet_service->sleep(*this, timeout);
    }

    void tasklet::yield()
    {
        tasklet_lock lock(this);
        _ptasklet_service->yield(*this);
    }

    void tasklet::start()
    {
        mutex_lock lock(_ptasklet_service->_mutex);
        if (!_start)
        {
            _ptasklet_service->push_tasklet(*this);
            _start = true;
        }
    }

    void tasklet::start_tasklet()
    {
        before_start_tasklet();
        try
        {
            if (_inherit)
            {
                wrapped_run();
            }
            else if (_func_ptr0)
            {
                _func_ptr0();
            }
            else if (_func_ptr1)
            {
                _func_ptr1(_arg);
            }
            else
            {
                throw exception(__FILE__, __LINE__);
            }
        }
        catch (stop_exception &e)
        {
            delete this;
            return;
        }
        catch (exception &e)
        {
            on_exception(e);
            return2threadlet();
        }
        catch (::std::exception &e)
        {
            on_exception(e);
            return2threadlet();
        }
        catch (...)
        {
            cerr << "[tasklet:" << hex << this << "] catch exception unknow" << endl;
            return2threadlet();
        }
        after_start_tasklet();
    }

    void tasklet::on_exception(exception &e)
    {
        cerr << "[tasklet:" << hex << this << "] catch exception: " << e << endl;
    }

    void tasklet::on_exception(::std::exception &e)
    {
        cerr << "[tasklet:" << hex << this << "] catch exception: " << e.what() << endl;
    }

    void tasklet::return2threadlet()
    {
        tasklet_lock lock(this);
        _ptasklet_service->return2threadlet(*this);
    }

    void tasklet::before_start_tasklet()
    {
        tasklet_lock lock(this);
        _ptasklet_service->before_start_tasklet(*this);
    }

    void tasklet::after_start_tasklet()
    {
        tasklet_lock lock(this);
        _ptasklet_service->after_start_tasklet(*this);
    }

    bool tasklet::parking()
    {
        return _ptasklet_service->parking(*this);
    }

    void tasklet::lock()
    {
        _pthreadlet->lock();
    }

    void tasklet::unlock()
    {
        _pthreadlet->unlock();
    }

} //namespace cerl
