#ifndef THROWABLE_HPP_INCLUDED
#define THROWABLE_HPP_INCLUDED

#include <string>
#include <iostream>

namespace cerl
{

    class throwable
    {
    };

    using std::string;

    class exception : public throwable
    {
    public:
        exception(const string file, int line)
            : _file(file), _line(line), _msg(NULL)
        {
        }
        exception(const string file, int line, const string msg)
            : _file(file), _line(line), _msg(msg)
        {
        }

        friend std::ostream &operator <<(std::ostream &output, const ::cerl::exception &e)
        {
            output.setf (std::ios::dec, std::ios::basefield);
            output << "::cerl::exception: " << e._file << ":" << e._line;
            if(e._msg.size())
            {
                output << " <- " << e._msg;
            }
            return output;
        }

    private:
        const string _file;
        int _line;
        const string _msg;
    };

    class error : public throwable
    {
    };

    class stop_exception : public throwable
    {
    };

    class close_exception : public throwable
    {
    };

} //namespace cerl

#endif // THROWABLE_HPP_INCLUDED
