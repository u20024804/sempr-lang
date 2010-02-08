#ifndef ATOMIC_HPP_INCLUDED
#define ATOMIC_HPP_INCLUDED

#include "lock.hpp"

namespace cerl
{

    template <typename T>
    class atomic
    {
    public:
        atomic<T>(T value_=0) :
                _value(value_),
                _spin()
        {
        }

        inline T operator++(int)
        {
            spin_lock _lock(_spin);
            return _value++;
        }
    private:
        T _value;
        spin _spin;
    };

    typedef atomic<int> atomic_int;
    typedef atomic<long> atomic_long;
    typedef atomic<unsigned int> atomic_uint;
    typedef atomic<unsigned long> atomic_ulong;

} //namespace cerl

#endif // ATOMIC_HPP_INCLUDED
