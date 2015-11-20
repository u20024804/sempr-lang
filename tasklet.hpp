#ifndef TASKLET_HPP_INCLUDED
#define TASKLET_HPP_INCLUDED

#include <ucontext.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <exception>

#include "constant.hpp"
#include "common.hpp"
#include "channel.hpp"
#include "ring_buffer.hpp"

namespace cerl
{

    class threadlet;
    class tasklet_service;

    enum tasklet_state
    {
        finished=-1, //一个tasklet一但完成就会被立刻删除
        no_start=0,
        running=1,
        runnable,
        waitting,
        timeout_waitting,
        sleeping,
        connecting,
        interrupted=-2,
        excepted=-3
    };

    enum net_state
    {
        serving = 0,
        listening = 1,
        connectting = 2
    };

    class tasklet;

    enum net_event
    {
        none = 0,
        net_read = 2,
        net_write = 4
    };

    struct netfile
    {
        netfile(tasklet &tasklet_, int fd, int recv_buff_size, int send_buff_size) :
                _fd(fd), _recv_buff(recv_buff_size), _to_read(0), _to_write(0),
                _send_buff(send_buff_size), _tasklet(tasklet_), _net_event(0), _net_state(serving) {}

        ~netfile()
        {
            close();
        }

        int on_recv()
        {
            int ret = _recv_buff.on_recv(_fd);
            if(ret > 0)
            {
                _to_read = ret >= _to_read ? 0 : _to_read - ret;
            }
            else
            {
                _to_read = 0;
            }
            return ret;
        }

        int on_send()
        {
            int ret = _send_buff.on_send(_fd);
            if(ret > 0)
            {
                _to_write = ret >= _to_write ? 0 : _to_write - ret;
            }
            else
            {
                _to_write = 0;
            }
            return ret;
        }

        void close()
        {
            _tasklet.close(*this);
        }

        int _fd;
        ring_buffer _recv_buff;
        ring_buffer _send_buff;
        int _to_read;
        int _to_write;
        tasklet &_tasklet;

        int _net_event;
        net_state _net_state;
    };

    class tasklet
    {
    public:
        static size_t default_stack_size;

        tasklet(tasklet_service& tasklet_service_, void (*handle_)(void), bool auto_start=true, size_t stack_size_=default_stack_size) :
                _ptasklet_service(&tasklet_service_),
                _func_ptr0(handle_),
                _func_ptr1(NULL),
                _arg(NULL),
                _channel(*_ptasklet_service, *this),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(stack_size_ > MIN_STACK_SIZE ? stack_size_ : MIN_STACK_SIZE),
                _stack(new char[_stack_size]),
                _start(false),
                _pthreadlet(NULL),
                _inherit(false),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
            init(auto_start);
        }

        tasklet(tasklet_service& tasklet_service_, void (*handle_)(void*), void *arg_, bool auto_start=true, size_t stack_size_=default_stack_size) :
                _ptasklet_service(&tasklet_service_),
                _func_ptr0(NULL),
                _func_ptr1(handle_),
                _arg(arg_),
                _channel(*_ptasklet_service, *this),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(stack_size_ > MIN_STACK_SIZE ? stack_size_ : MIN_STACK_SIZE),
                _stack(NULL),
                _start(false),
                _pthreadlet(NULL),
                _inherit(false),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
            init(auto_start);
        }

        tasklet(tasklet_service& tasklet_service_, void (*handle_)(tasklet*), bool auto_start=true, size_t stack_size_=default_stack_size) :
                _ptasklet_service(&tasklet_service_),
                _func_ptr0(NULL),
                _func_ptr1((void(*)(void *))handle_),
                _arg(this),
                _channel(*_ptasklet_service, *this),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(stack_size_ > MIN_STACK_SIZE ? stack_size_ : MIN_STACK_SIZE),
                _stack(NULL),
                _start(false),
                _pthreadlet(NULL),
                _inherit(false),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
            init(auto_start);
        }

        tasklet(tasklet_service& tasklet_service_, void (*handle_)(tasklet&), bool auto_start=true, size_t stack_size_=default_stack_size) :
                _ptasklet_service(&tasklet_service_),
                _func_ptr0(NULL),
                _func_ptr1((void(*)(void *))handle_),
                _arg(this),
                _channel(*_ptasklet_service, *this),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(stack_size_ > MIN_STACK_SIZE ? stack_size_ : MIN_STACK_SIZE),
                _stack(NULL),
                _start(false),
                _pthreadlet(NULL),
                _inherit(false),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
            init(auto_start);
        }

        tasklet(tasklet_service& tasklet_service_, bool auto_start=true, size_t stack_size_=default_stack_size) :
                _ptasklet_service(&tasklet_service_),
                _func_ptr0(NULL),
                _func_ptr1(NULL),
                _arg(NULL),
                _channel(*_ptasklet_service, *this),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(stack_size_ > MIN_STACK_SIZE ? stack_size_ : MIN_STACK_SIZE),
                _stack(NULL),
                _start(false),
                _pthreadlet(NULL),
                _inherit(true),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
            init(auto_start);
        }

        tasklet(size_t stack_size_) :
                _ptasklet_service(NULL),
                _func_ptr0(NULL),
                _func_ptr1(NULL),
                _arg(NULL),
                _channel(*_ptasklet_service, *this),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(stack_size_ > MIN_STACK_SIZE ? stack_size_ : MIN_STACK_SIZE),
                _stack(NULL),
                _start(false),
                _pthreadlet(NULL),
                _inherit(true),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
        }

        tasklet() :
                _ptasklet_service(NULL),
                _func_ptr0(NULL),
                _func_ptr1(NULL),
                _arg(NULL),
                _channel(),
                _id(_channel.id()),
                _ucontext(),
                _stack_size(default_stack_size > MIN_STACK_SIZE ? default_stack_size: MIN_STACK_SIZE),
                _stack(NULL),
                _start(false),
                _pthreadlet(NULL),
                _inherit(true),
                _prev_waitting(NULL),
                _next_waitting(NULL),
                _next_runnable(NULL),
                _state(no_start),
                _wake(0)
        {
        }

        virtual ~tasklet()
        {
            if (_stack)
            {
                delete[] _stack;
                _stack = NULL;
            }
        }
        void send(tasklet& target, const message& msg);
        void send(tasklet* ptarget, const message& msg)
        {
            send(*ptarget, msg);
        }
        template <typename Tasklet>
        void send(Tasklet* ptarget, const message& msg)
        {
            send((tasklet *)ptarget, msg);
        }
        template <typename Tasklet>
        void send(Tasklet& target, const message& msg)
        {
            send(&target, msg);
        }
        message send(netfile &netfile_, const void * const buf, size_t len, double timeout=infinity);
        message recv(netfile &netfile_, const void * const buf, size_t len, double timeout=infinity);
        int port(const sockaddr_in *addr, int backlog=128, int socket_type=SOCK_STREAM, int protocol=0);
        int accept(sockaddr * addr=NULL, socklen_t * addrlen=NULL);
        int connect(const struct sockaddr *addr, double timout=infinity);
        void close(netfile &netfile_);
        void close(int fd);
        message recv(double timeout=infinity);
        void sleep(double timeout);
        void yield();
        void start();

        void stack_size(size_t stack_size_)
        {
            _stack_size = stack_size_;
        }

        size_t stack_size() const
        {
            return _stack_size;
        }

        const cid& id() const
        {
            return _id;
        }

        tasklet_state state() const
        {
            return _state;
        }

        struct Comp
        {
            bool operator()(const tasklet * const self, const tasklet* const other) const
            {
                return self->_wake < other->_wake;
            }
        };

    protected:
        friend class tasklet_service;
        friend class threadlet;

        void before_start_tasklet();
        void after_start_tasklet();
        void start_tasklet(void *);
        void return2threadlet();
        virtual void on_exception(exception& e);
        virtual void on_exception(::std::exception& e);

        tasklet_service *_ptasklet_service;
        void (*_func_ptr0)(void);
        void (*_func_ptr1)(void *);
        void *_arg;
        channel _channel;

        inline void init(bool auto_start)
        {
            if (getcontext(&_ucontext) == -1)
            {
                throw exception(__FILE__, __LINE__);
            }
            if (!_stack)
            {
                _stack = new char[_stack_size];
            }
            _ucontext.uc_stack.ss_sp = _stack;
            _ucontext.uc_stack.ss_size = _stack_size;
            _ucontext.uc_link = NULL;
            //makecontext的可变参数部分，在传递指针的时候，可能在64位系统下有问题
            //因为可变参数的类型一般都需要与int的size一样大
            makecontext(&_ucontext, (void(*)(void))&tasklet::start_tasklet, 2, this, this);
            if (auto_start)
            {
                start();
            }
        }

        bool parking();
        void lock();
        void unlock();
        friend class quick_lock<tasklet>;
        friend class timer;
        friend class port_service;

        virtual void run(){}

    private:
        void wrapped_run()
        {
            run();
        }

        cid _id;
        ucontext_t _ucontext;
        size_t _stack_size;
        char* _stack;
        bool _start;
        threadlet* _pthreadlet;
        bool _inherit;
        tasklet *_prev_waitting;
        tasklet *_next_waitting;
        tasklet *_next_runnable;
        tasklet_state _state;
        unsigned long long _wake;
    };
    typedef quick_lock<tasklet> tasklet_lock;

} //namespace cerl

#endif // TASKLET_HPP_INCLUDED
