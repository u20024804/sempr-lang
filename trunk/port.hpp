#ifndef PORT_INCLUDED
#define PORT_INCLUDED

namespace cerl
{

    class port
    {
    public:
        virtual void open() = 0;
        virtual void read() = 0;
        virtual void write() = 0;
        virtual void close() = 0;
        virtual ~port{}
    };

} //namespace cerl

#endif // PORT_INCLUDED
