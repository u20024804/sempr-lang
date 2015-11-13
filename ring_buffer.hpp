//
// Created by QingqingPan on 11/13/15.
//

#ifndef RING_BUFFER_HPP
#define RING_BUFFER_HPP

#include <string.h>

class ring_buffer
{
public:
    ring_buffer(int size) : _buffer(new char[size]), _head(0), _tail(0), _size(size)
    {
    }

    ~ring_buffer()
    {
        if(_buffer)
        {
            delete[] _buffer;
            _buffer = NULL;
        }
    }

    inline int readable()
    {
        return _tail >= _head ? _tail - _head : _size - _head + _tail;
    }

    inline int writable()
    {
        return (_head > _tail || (_head == 0 && _tail == 0)) ? _head - _tail : _size - _tail + _head;
    }

    int read(char *target, const int len, bool all=true)
    {
        if(len <= 0)
        {
            return len == 0 ? 0 : -3;
        }

        const int size = readable();
        if(all && len > size)
        {
            return len <= _size ? -1 : -2;
        }
        else if(size == 0)
        {
            return -4;
        }

        const int to_read = size > len ? len : size;

        if(_tail > _head)
        {
            memcpy(target, _buffer + _head, to_read);
            _head += to_read;
         }
        else
        {
            const int part1 = _size - _head;
            if(to_read <= part1)
            {
                memcpy(target, _buffer + _head, to_read);
                _head = to_read == part1 ? 0 : _head + to_read;
            }
            else
            {
                memcpy(target, _buffer + _head, part1);
                const int rest = to_read - part1;
                memcpy(target + part1, _buffer, rest);
                _head = rest;
            }
        }

        if(size == len)
        {
            _head = 0;
            _tail = 0;
        }

        return to_read;
    }

    int write(char *target, int len, bool all=true)
    {
        if(len <= 0)
        {
            return len == 0 ? 0 : -3;
        }

        const int size = writable();
        if(all && len > size)
        {
            return len <= _size ? -1 : -2;
        }
        else if(size == 0)
        {
            return -4;
        }

        const int to_write = len > size ? size : len;

        if(_head >= _tail)
        {
            memcpy(_buffer + _tail, target, to_write);
            _tail += to_write;
        }
        else
        {
            const int part1 = _size - _tail;
            if(part1 >= to_write)
            {
                memcpy(_buffer + _tail, target, to_write);
                _tail += to_write;
            }
            else
            {
                if(part1 > 0)
                {
                    memcpy(_buffer + _tail, target, part1);
                }
                const int rest = to_write - part1;
                memcpy(_buffer, target + part1, rest);
                _tail = rest;
            }
        }

        return to_write;
    }

private:
    const char *_buffer;
    int _head;
    int _tail;
    const _size;
};

#endif //RING_BUFFER_HPP
