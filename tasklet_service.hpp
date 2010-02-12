#ifndef TASKET_SERVICE_HPP_INCLUDED
#define TASKET_SERVICE_HPP_INCLUDED

#include <assert.h>
#include <vector>
#include <set>

#include "common.hpp"
#include "atomic.hpp"
#include "channel.hpp"
#include "lock.hpp"
#include "condition.hpp"
#include "tasklet.hpp"
#include "threadlet.hpp"

namespace cerl
{

    using std::vector;
    using std::set;

    class cid;
    class timer;
    class port_service;

    class tasklet_service
    {
    public:
        tasklet_service(int pool_size_=1, bool need_port_service=true, int maxevents=DEFAULT_MAXEVENTS);
        virtual ~tasklet_service();

        void start();
        void join();

        int pool_size()
        {
            mutex_lock lock(_mutex);
            return _pool_size;
        }

        void pool_size(int pool_size_)
        {
            mutex_lock lock(_mutex);
            _pool_size = pool_size_;
            //TODO
        }

        bool stopped()
        {
            mutex_lock lock(_mutex);
            return _stop;
        }
        void stop(bool stop_=true);

        void add(tasklet *ptasklet_)
        {
            ptasklet_->_ptasklet_service = this;
            ptasklet_->_channel._ptasklet = ptasklet_;
            ptasklet_->_channel._pchannel_manager = &this->_channel_manager;
            ptasklet_->_channel.init();
            ptasklet_->_id = ptasklet_->_channel.id();
            ptasklet_->init(true);
        }

        template<typename Tasklet>
        void add(Tasklet *ptasklet_)
        {
            add((tasklet *)ptasklet_);
        }

        void add(tasklet& tasklet_)
        {
            add(&tasklet_);
        }

    protected:
        friend class tasklet;
        friend class channel_manager;
        friend class threadlet;

        void send(tasklet& target, tasklet& sender, const message& msg);
        message recv(tasklet& recver, double timeout);
        message recv(tasklet& recver);
        void sleep(tasklet& tasklet_, double timeout);
        void yield(tasklet& tasklet_);
        tasklet& pop_tasklet();
        void active_tasklet(tasklet& tasklet_, bool delete_from_sleepers=true);

        void before_start_tasklet(tasklet& tasklet_);
        void after_start_tasklet(tasklet& tasklet_);

        void park(tasklet& tasklet_)
        {
            tasklet_._state = waitting;
            if (_head_waittings == NULL)
            {
                assert(_tail_waittings == NULL);
                _head_waittings = &tasklet_;
            }
            else if (_head_waittings == _tail_waittings)
            {
                _head_waittings->_next_waitting = &tasklet_;
                tasklet_._prev_waitting = _head_waittings;
            }
            else
            {
                _head_waittings->_next_waitting = &tasklet_;
                tasklet_._prev_waitting = _head_waittings;
                tasklet_._next_waitting = _tail_waittings;
                _tail_waittings->_prev_waitting = &tasklet_;
            }
            _tail_waittings = &tasklet_;
        }

        void unpark(tasklet& tasklet_)
        {
            assert(_head_waittings != NULL);
            assert(_tail_waittings != NULL);
            if (_head_waittings == _tail_waittings)
            {
                _head_waittings = NULL;
                _tail_waittings = NULL;
            }
            else if (tasklet_._next_waitting == NULL)
            {
                _tail_waittings = tasklet_._prev_waitting;
                _tail_waittings->_next_waitting = NULL;
            }
            else if (tasklet_._prev_waitting == NULL)
            {
                _head_waittings = tasklet_._next_waitting;
                _head_waittings->_prev_waitting = NULL;
            }
            else
            {
                tasklet_._prev_waitting->_next_waitting = tasklet_._next_waitting;
                tasklet_._next_waitting ->_prev_waitting = tasklet_._prev_waitting;
            }
            tasklet_._prev_waitting = NULL;
            tasklet_._next_waitting = NULL;
            tasklet_._state = runnable;
        }

        bool parking(tasklet& tasklet_)
        {
            if (tasklet_._prev_waitting || tasklet_._next_waitting)
            {
                return true;
            }
            if (_head_waittings == &tasklet_)
            {
                assert(_tail_waittings == &tasklet_);
                assert(tasklet_._state == waitting);
                return true;
            }
            return false;
        }

        bool empty_runnables()
        {
            if (!_head_runnables)
            {
                assert(_tail_runnables == NULL);
                return true;
            }
            return false;
        }

        void push_runnable(tasklet& tasklet_)
        {
            tasklet_._state = runnable;
            if (_head_runnables == NULL)
            {
                assert(_tail_runnables == NULL);
                _head_runnables = &tasklet_;
            }
            else
            {
                _tail_runnables->_next_runnable = &tasklet_;
            }
            _tail_runnables = &tasklet_;
        }

        tasklet& pop_runnable()
        {
            assert(_head_runnables != NULL);
            assert(_tail_runnables != NULL);
            tasklet& tasklet_ = *_head_runnables;
            if (_head_runnables == _tail_runnables)
            {
                _head_runnables = NULL;
                _tail_runnables = NULL;
            }
            else
            {
                _head_runnables = _head_runnables->_next_runnable;
            }
            tasklet_._next_runnable = NULL;
            tasklet_._state = running;
        }

        unsigned long long now();

        bool add_read(tasklet &tasklet_, int fd);
        bool add_write(tasklet &tasklet_, int fd);
        void shutdown(tasklet &tasklet_, int fd);

    private:
        mutex _mutex;
        condition _condition;
        friend channel::channel(tasklet_service &tasklet_service_, tasklet& tasklet_) ;
        channel_manager _channel_manager;
        atomic_uint _idgen;
        int _pool_size;
        vector<threadlet> _threadlets;
        tasklet* _head_runnables;
        tasklet* _tail_runnables;
        tasklet* _head_waittings;
        tasklet* _tail_waittings;
        bool _stop;
        timer *_ptimer;
        set<tasklet*, tasklet::Comp> _sleepers;
        port_service *_pport_service;
        bool _joined;

        void switch_tasklet(tasklet& current, tasklet& next);
        void switch_tasklet(tasklet& tasklet_);
        void return2threadlet(tasklet& tasklet_);

        void push_tasklet(tasklet& tasklet_)
        {
            bool empty = empty_runnables();
            push_runnable(tasklet_);
            if (empty)
            {
                _condition.notify();
            }
        }
        bool check_waittings();
        friend void tasklet::start();
        friend class timer;
        friend class port_service;
    };

} //namespace cerl

#endif // TASKET_SERVICE_HPP_INCLUDED
