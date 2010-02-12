#include <iostream>
#include <ucontext.h>
#include <sys/socket.h>

#include "constant.hpp"
#include "common.hpp"
#include "tasklet.hpp"
#include "threadlet.hpp"
#include "tasklet_service.hpp"

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

    tasklet::on_port_finish::~on_port_finish()
    {
        _tasklet._buffer = NULL;
        _tasklet._buffer_size = 0;
        _tasklet._flags = 0;
    }

    int tasklet::send(int fd, const void *buf, size_t len, int flags)
    {
        on_port_finish on_port_finish_(this, fd, on_port_finish::op_read);
        _buffer = (char *)buf;
        _buffer_size = len;
        _flags = flags;
        bool success = _ptasklet_service->add_write(*this, fd);
        if (!success)
        {
            return -1;
        }
        message msg = recv();
        if (msg.type != port_msg || msg.content.ivalue < -1)
        {
            throw exception();
        }
        return msg.content.ivalue;
    }

    int tasklet::recv(int fd, void *buf, size_t len, int flags)
    {
        on_port_finish on_port_finish_(this, fd, on_port_finish::op_read);
        _buffer = (char *)buf;
        _buffer_size = len;
        _flags = flags;
        bool success = _ptasklet_service->add_read(*this, fd);
        if (!success)
        {
            return -1;
        }
        message msg = recv();
        if (msg.type != port_msg || msg.content.ivalue < -1)
        {
            throw exception();
        }
        return msg.content.ivalue;
    }

    void tasklet::shutdown(int fd)
    {
        _ptasklet_service->shutdown(*this, fd);
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
                throw exception();
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
