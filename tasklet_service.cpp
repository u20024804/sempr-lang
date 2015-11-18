#include <assert.h>
#include <ucontext.h>
#include <unistd.h>

#include "tasklet.hpp"
#include "tasklet_service.hpp"
#include "timer.hpp"
#include "port_service.hpp"

namespace cerl
{

    tasklet_service::tasklet_service(int pool_size_, bool need_port_service, int maxevents):
            _mutex(),
            _condition(_mutex),
            _channel_manager(*this),
            _idgen(),
            _pool_size(pool_size_),
            _threadlets(_pool_size),
            _head_runnables(NULL),
            _tail_runnables(NULL),
            _head_waittings(NULL),
            _tail_waittings(NULL),
            _stop(false),
            _ptimer(new timer(*this)),
            _sleepers(),
            _pport_service(need_port_service ? new port_service(*this, maxevents) : NULL),
            _joined(false)
    {
        for (vector<threadlet>::iterator it = _threadlets.begin(); it != _threadlets.end(); it++)
        {
            it->_ptasklet_service = this;
            it->_pmutex = &_mutex;
        }
    }

    tasklet_service::~tasklet_service()
    {
        join();
        tasklet* ptasklet = _head_runnables;
        while(ptasklet)
        {
            tasklet* pnext = ptasklet->_next_runnable;
            delete ptasklet;
            ptasklet = pnext;
        }
        ptasklet = _head_waittings;
        while(ptasklet)
        {
            tasklet* pnext = ptasklet->_next_waitting;
            delete ptasklet;
            ptasklet = pnext;
        }
        for(set<tasklet*, tasklet::Comp>::iterator it = _sleepers.begin(); it != _sleepers.end(); ++it)
        {
            ptasklet = *it;
            delete ptasklet;
        }
        if(_pport_service)
        {
            delete _pport_service;
        }
        delete _ptimer;
    }

    void tasklet_service::stop(bool stop_)
    {
        mutex_lock lock(_mutex);
        _stop = stop_;
        if(_stop == false)
        {
            return;
        }
        for (vector<threadlet>::iterator it = _threadlets.begin(); it != _threadlets.end(); it++)
        {
            it->stop(stop_);
        }
        _condition.notify_all();
        if(_pport_service)
        {
            _pport_service->stop();
        }
        _ptimer->stop();
    }

    void tasklet_service::send(tasklet& target, tasklet& sender, const message& msg)
    {
        if(_stop)
        {
            throw stop_exception();
        }
        _channel_manager.dispatch(target._channel, msg);
        yield(sender);
    }

    message  tasklet_service::recv(tasklet& recver)
    {
        if(_stop)
        {
            throw stop_exception();
        }
        bool switched = false;
        while (_channel_manager.empty(recver._channel))
        {
            park(recver);
            if (empty_runnables())
            {
                switch_tasklet(recver);
            }
            else
            {
                switch_tasklet(recver, pop_runnable());
            }
            switched = true;
        }

        if (!switched)
        {
            yield(recver);
        }
        return _channel_manager.fetch(recver._channel);
    }

    message  tasklet_service::recv(tasklet& recver, double timeout)
    {
        if(_stop)
        {
            throw stop_exception();
        }
        if (timeout == infinity)
        {
            return recv(recver);
        }

        bool switched = false;
        unsigned long long wake_ = now()+(int)(timeout*10);
        while (_channel_manager.empty(recver._channel))
        {
            recver._state = timeout_waitting;
            recver._wake = wake_;
            _sleepers.insert(&recver);
            if (empty_runnables())
            {
                switch_tasklet(recver);
            }
            else
            {
                switch_tasklet(recver, pop_runnable());
            }
            switched = true;
    }

        if (!switched)
        {
            yield(recver);
        }
        return _channel_manager.fetch(recver._channel);
    }

    void tasklet_service::sleep(tasklet& tasklet_, double timeout)
    {
        if(_stop)
        {
            throw stop_exception();
        }
        tasklet_._state = sleeping;
        tasklet_._wake = now()+(int)(timeout*10);
        _sleepers.insert(&tasklet_);
        if (empty_runnables())
        {
            switch_tasklet(tasklet_);
        }
        else
        {
            tasklet& next_tasklet = pop_runnable();
            switch_tasklet(tasklet_, next_tasklet);
        }
    }

    void tasklet_service::yield(tasklet& tasklet_)
    {
        if(_stop)
        {
            throw stop_exception();
        }
        if (empty_runnables())
        {
            return;
        }
        push_runnable(tasklet_);
        assert(!empty_runnables());
        tasklet& next = pop_runnable();
        switch_tasklet(tasklet_, next);
    }

    inline void tasklet_service::switch_tasklet(tasklet& current, tasklet& next)
    {
        threadlet * const pthreadlet_ = current._pthreadlet;
        current._pthreadlet = NULL;
        next._pthreadlet = pthreadlet_;
        pthreadlet_->_pcurrent_tasklet = &next;

        next._state = running;
        swapcontext(&current._ucontext, &next._ucontext);
        current._state = running;
    }

    inline void tasklet_service::switch_tasklet(tasklet& current)
    {
        threadlet& threadlet_ = *current._pthreadlet;
        current._pthreadlet = NULL;
        swapcontext(&current._ucontext, &threadlet_._ucontext);
        current._state = running;
    }

    void tasklet_service::return2threadlet(tasklet& tasklet_)
    {
        tasklet_._pthreadlet->_plast_finished_tasklet = &tasklet_;
        switch_tasklet(tasklet_);
        assert(false);
    }

    void tasklet_service::start()
    {
        if(_pport_service)
        {
            _pport_service->start();
        }
        _ptimer->start();
        for (vector<threadlet>::iterator it = _threadlets.begin(); it != _threadlets.end(); it++)
        {
            it->start();
        }
    }

    void tasklet_service::join()
    {
        if (_joined)
        {
            return;
        }
        for (vector<threadlet>::iterator it = _threadlets.begin(); it != _threadlets.end(); it++)
        {
            if (it->started())
            {
                //如果已经启动了，但是又退出了会怎样？
                try
                {
                    it->join();
                }
                catch(const exception& e)
                {
                }
            }
        }
        if(_pport_service && _pport_service->started())
        {
            _pport_service->join();
        }
        if (_ptimer->started())
        {
            _ptimer->join();
        }
        _joined = true;
    }

    void tasklet_service::before_start_tasklet(tasklet& tasklet_)
    {
        if(_stop)
        {
            throw stop_exception();
        }
    }

    void tasklet_service::after_start_tasklet(tasklet& tasklet_)
    {
        threadlet * const pthreadlet_ = tasklet_._pthreadlet;
        pthreadlet_->_plast_finished_tasklet = &tasklet_;
        tasklet_._state = finished;
        swapcontext(&tasklet_._ucontext, &pthreadlet_->_ucontext);
    }

    tasklet& tasklet_service::pop_tasklet()
    {
        tasklet* ptasklet = NULL;
        while (!ptasklet && !_stop)
        {
            if (empty_runnables())
            {
                //这里本来可能引发因为_condition.wait释放_mutex导致的线程安全问题
                //但是很幸运的是，pop_tasklet只被threadlet::loop调用，这样就正好不会引发线程安全问题了
                _condition.wait();
            }
            else
            {
                assert(!empty_runnables());
                ptasklet = &pop_runnable();
            }
        }
        if (_stop && !ptasklet)
        {
            throw stop_exception();
        }
        return *ptasklet;
    }

    void tasklet_service::active_tasklet(tasklet& tasklet_, bool delete_from_sleepers)
    {
        if (tasklet_._state == waitting)
        {
            unpark(tasklet_);
        }
        else if (tasklet_._state == timeout_waitting)
        {
            tasklet_._wake = 0;
            if (delete_from_sleepers)
            {
                _sleepers.erase(&tasklet_);
            }
        }
        else
        {
            throw exception(__FILE__, __LINE__);
        }
        push_tasklet(tasklet_);
    }

    bool tasklet_service::check_waittings()
    {
        if (_head_waittings == NULL)
        {
            assert(_tail_waittings == NULL);
            return false;
        }
        bool found = false;
        for (tasklet *it = _head_waittings; it != NULL;)
        {
            tasklet *next = it->_next_waitting;
            if (!_channel_manager.empty(it->_channel))
            {
                unpark(*it);
                push_runnable(*it);
                //这里是不是需要notify或者notify_all一下其他等待的threadlet呢？
                found = true;
            }
            it = next;
        }
        return found;
    }

    unsigned long long tasklet_service::now()
    {
        return _ptimer->time();
    }

    bool tasklet_service::add_read(netfile &netfile_)
    {
        return _pport_service->add_read(netfile_);
    }

    bool tasklet_service::add_write(netfile &netfile_)
    {
        return _pport_service->add_write(netfile_);
    }

    void tasklet_service::close(netfile &netfile_)
    {
        _pport_service->on_finish(tasklet_);
        ::close(tasklet_._fd);
        tasklet_._fd = -1;
    }

} //namespace cerl
