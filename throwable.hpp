#ifndef THROWABLE_HPP_INCLUDED
#define THROWABLE_HPP_INCLUDED

#include <iostream>

namespace cerl
{

    class throwable
    {
    };

    class exception : throwable
    {
    public:
        friend std::ostream &operator <<(std::ostream &output, const ::cerl::exception &e)
        {
            output << "::cerl::exception";
            return output;
        };
    };

    class error : throwable
    {
    };

    class stop_exception : exception
    {
    };

} //namespace cerl

#endif // THROWABLE_HPP_INCLUDED
