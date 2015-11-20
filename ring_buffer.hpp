//
// Created by QingqingPan on 11/13/15.
//

#ifndef RING_BUFFER_HPP
#define RING_BUFFER_HPP

#include <string.h>
#include <unistd.h>
#include <errno.h>

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
            delete[] (char *)_buffer;
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

    int read(void *target, const int len, bool all=true)
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

    int on_recv(int fd)
    {
        int to_read = writable();
        if(to_read <= 0)
        {
            return 0;
        }

        if(_head > _tail)
        {
            for(int i = 0; i < 10; i++)
            {
                int ret = ::read(fd, _buffer + _tail, to_read);
                if(ret == 0)
                {
                    return 0;
                }
                else if(ret < 0)
                {
                    if(ret != -1)
                    {
                        return ret;
                    }
                    if(errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        return ret;
                    }
                }
                else
                {
                    _tail += ret;
                    return ret;
                }
            }
        }
        else
        {
            int ret = -1;
            for(int i = 0; i < 10; i++)
            {
                ret = ::read(fd, _buffer + _tail, _size - _tail);
                if(ret == 0)
                {
                    return 0;
                }
                else if(ret < 0)
                {
                    if(ret != -1)
                    {
                        return ret;
                    }
                    if(errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        return ret;
                    }
                }
                else
                {
                    break;
                }
            }

            if(ret <= 0)
            {
                return ret;
            }
            if(ret < _size - _tail)
            {
                _tail += ret;
                return ret;
            }

            const int part1 = ret;
            ret = -1;

            for(int i = 0; i < 10; i++)
            {
                ret = ::read(fd, _buffer, _head);
                if(ret == 0)
                {
                    break;
                }
                else if(ret < 0)
                {
                    if(ret != -1)
                    {
                        break;
                    }
                    if(errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if(ret <= 0)
            {
                _tail = _size;
                return part1;
            }
            _tail = ret;
            return part1 + ret;
        }
        return 0;
    }

    int write(void *target, int len, bool all=true)
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

    void back_write(int go_back, void *target, int len)
    {
        int pos = _tail - go_back;
        if(pos < 0)
        {
            pos += _size;
        }

        int current_pos = _tail;
        _tail = pos;
        write(target, len);
        _tail = current_pos;
    }

    int on_send(int fd)
    {
        int to_write = readable();
        if(to_write <= 0)
        {
            return 0;
        }

        if(_head < _tail)
        {
            for(int i = 0; i < 10; i++)
            {
                int ret = ::write(fd, _buffer + _head, to_write);
                if(ret == 0)
                {
                    return 0;
                }
                else if(ret < 0)
                {
                    if(ret != -1)
                    {
                        return ret;
                    }
                    if(errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        return ret;
                    }
                }
                else
                {
                    _head += ret;
                    return ret;
                }
            }
        }
        else
        {
            int ret = -1;
            for(int i = 0; i < 10; i++)
            {
                ret = ::write(fd, _buffer + _head, _size - _head);
                if(ret == 0)
                {
                    return 0;
                }
                else if(ret < 0)
                {
                    if(ret != -1)
                    {
                        return ret;
                    }
                    if(errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        return ret;
                    }
                }
                else
                {
                    break;
                }
            }

            if(ret <= 0)
            {
                return  ret;
            }
            else if(ret < _size - _head)
            {
                _head += ret;
                return ret;
            }

            const int part1 = ret;
            ret = -1;

            for(int i = 0; i < 10; i++)
            {
                ret = ::write(fd, _buffer, _tail);
                if(ret == 0)
                {
                    break;
                }
                else if(ret < 0)
                {
                    if(ret != -1)
                    {
                        break;
                    }
                    if(errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if(ret <= 0)
            {
                _head = 0;
                return part1;
            }
            _head = ret;
            return part1 + ret;

        }
        return 0;
    }

private:
    void * const _buffer;
    int _head;
    int _tail;
    const _size;
};

#endif //RING_BUFFER_HPP
