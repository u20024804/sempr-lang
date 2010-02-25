#ifndef MESSAGE_HPP_INCLUDED
#define MESSAGE_HPP_INCLUDED

#include <string>
#include <map>
#include <queue>

#include "common.hpp"
#include "atomic.hpp"

namespace cerl
{

    using std::map;
    using std::string;
    using std::queue;

    class tasklet;
    class tasklet_service;

    enum message_type {no_msg=-1, empty_msg=0, msg_int=1, msg_char, msg_void, msg_string, port_msg=-9};
    union message_context
    {
        int ivalue;
        char *cvalue;
        void *pvalue;
        string *svalue;
    };
    struct message
    {
        message_context content;
        message_type type;
    };

    class messagebox
    {
    public:
        messagebox() : _messages() {}

        void push(const message& msg)
        {
            _messages.push(msg);
        }
        bool empty() const
        {
            return _messages.empty();
        }
        message pop()
        {
            message msg = _messages.front();
            _messages.pop();
            return msg;
        }
    private:
        queue<message> _messages;
    };

    class channel_manager;

    class cid
    {
    public:
        cid() : _id(-1) {}

        long id() const
        {
            return _id;
        }

        bool operator<(const cid& cid_) const
        {
            return this->_id < cid_._id;
        }
        bool operator==(const cid& cid_) const
        {
            return this->_id == cid_._id;
        }
    private:
        long _id;

        friend class channel_manager;
    };

    class channel
    {
    public:
        channel();
        channel(tasklet_service &tasklet_service_, tasklet& tasklet_) ;
        virtual ~channel();

        const cid& id () const
        {
            return _id;
        }
    private:
        friend class channel_manager;
        friend class tasklet_service;
        messagebox _message_box;
        tasklet *_ptasklet;
        cid _id;
        channel_manager *_pchannel_manager;
        spin _spin;

        void init();
    };

    class channel_manager
    {
    public:
        channel_manager(tasklet_service& tasklet_service_) :
                _tasklet_service(tasklet_service_),
//                _channels(),
                _spin()
        {
        }
        void init_channel(channel& channel_);
        void dispatch(channel &channel_, const message& msg, bool delete_from_sleepers=true);
        bool empty(channel &channel_)
        {
            spin_lock lock(channel_._spin);
            return channel_._message_box.empty();
        }
        const message fetch(channel &channel_)
        {
            spin_lock lock(channel_._spin);
            return channel_._message_box.pop();
        }
//        void remove(channel& channel_);

    private:
        tasklet_service& _tasklet_service;
//        map<cid, channel*> _channels;
        spin _spin;
    };

    class sequence
    {
    public:
        sequence() : _long(0) {}

        long next_value()
        {
            return _long++;
        }

    private:
        atomic_long _long;
    };

    extern sequence global_sequence;

} //namespace cerl

#endif // MESSAGE_HPP_INCLUDED
