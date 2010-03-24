#include "channel.hpp"
#include "tasklet_service.hpp"

namespace cerl
{
    sequence global_sequence;

    channel::channel(tasklet_service &tasklet_service_, tasklet& tasklet_) :
        _message_box(),
        _ptasklet(&tasklet_),
        _id(),
        _pchannel_manager(&tasklet_service_._channel_manager),
        _spin()
    {
        init();
    }
    channel::channel() :
        _message_box(),
        _ptasklet(NULL),
        _id(),
        _pchannel_manager(NULL),
        _spin()
    {
    }

    channel::~channel()
    {
//        _pchannel_manager->remove(*this);
    }

    void channel::init()
    {
        _pchannel_manager->init_channel(*this);
    }

    void channel_manager::init_channel(channel& channel_)
    {
        channel_._id._id = global_sequence.next_value();
    }

    void channel_manager::dispatch(channel &channel_, const message& msg, bool delete_from_sleepers)
    {
        {
            spin_lock lock(channel_._spin);
            channel_._message_box.push(msg);
        }
        if(channel_._ptasklet->state() == waitting || channel_._ptasklet->state() == timeout_waitting)
        {
            _tasklet_service.active_tasklet(*channel_._ptasklet, delete_from_sleepers);
        }
    }

}
