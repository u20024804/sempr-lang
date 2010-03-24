#ifndef LOG_HPP_INCLUDED
#define LOG_HPP_INCLUDED

#include <string>
#include <iostream>
#include <fstream>

#include <log4cplus/loglevel.h>
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>

namespace cerl
{
    using namespace log4cplus;
    using std::string;
    using std::ifstream;
    using std::cerr;
    using std::endl;
    using std::clog;

    enum log_level
    {
        TRACE=TRACE_LOG_LEVEL,
        DEBUG=DEBUG_LOG_LEVEL,
        INFO=INFO_LOG_LEVEL,
        WARN=WARN_LOG_LEVEL,
        ERROR=ERROR_LOG_LEVEL,
        FATAL=FATAL_LOG_LEVEL,
        OFF=OFF_LOG_LEVEL,
        ALL=ALL_LOG_LEVEL
    };

    class configurator
    {
    public:
        static const char * file_name;
        configurator()
        {
            ifstream in;
            in.open(file_name, ifstream::in);
            if(in.fail())
            {
                cerr << "log4cplus:ERROR Please initialize the log4cplus system properly." << endl;
                BasicConfigurator::doConfigure();
            }
            else
            {
                PropertyConfigurator::doConfigure(file_name);
            }
            in.close();
        }
    private:
    };

    class logger
    {
    public:
        static configurator global_configurator;

        template<typename MSG>
        inline static logger instance(MSG name)
        {
            return logger(name);
        }

        inline bool trace()
        {
            return _logger.isEnabledFor(log4cplus::TRACE_LOG_LEVEL);
        }
        inline bool debug()
        {
            return _logger.isEnabledFor(log4cplus::DEBUG_LOG_LEVEL);
        }
        inline bool info()
        {
            return _logger.isEnabledFor(log4cplus::INFO_LOG_LEVEL);
        }
        inline bool warn()
        {
            return _logger.isEnabledFor(log4cplus::WARN_LOG_LEVEL);
        }
        inline bool error()
        {
            return _logger.isEnabledFor(log4cplus::ERROR_LOG_LEVEL);
        }
        inline bool fatal()
        {
            return _logger.isEnabledFor(log4cplus::FATAL_LOG_LEVEL);
        }

        template<typename MSG>
        inline void trace(MSG msg)
        {
            LOG4CPLUS_TRACE(_logger, msg);
        }

        template<typename MSG>
        inline void debug(MSG msg)
        {
            LOG4CPLUS_DEBUG(_logger, msg);
        }

        template<typename MSG>
        inline void info(MSG msg)
        {
            LOG4CPLUS_INFO(_logger, msg);
        }

        template<typename MSG>
        inline void warn(MSG msg)
        {
            LOG4CPLUS_WARN(_logger, msg);
        }

        template<typename MSG>
        inline void error(MSG msg)
        {
            LOG4CPLUS_ERROR(_logger, msg);
        }

        template<typename MSG>
        inline void fatal(MSG msg)
        {
            LOG4CPLUS_FATAL(_logger, msg);
        }

        template<typename L>
        inline void log_level(L level)
        {
            _logger.setLogLevel(level);
        }

        inline ::cerl::log_level log_level()
        {
            LogLevel level = _logger.getLogLevel();
            switch(level)
            {
                case OFF:
                    return OFF;
                case FATAL:
                    return FATAL;
                case ERROR:
                    return ERROR;
                case WARN:
                    return WARN;
                case INFO:
                    return INFO;
                case DEBUG:
                    return DEBUG;
                case TRACE:
                    return TRACE;
                default:
                    return ALL;
            }
        }

    private:
        Logger _logger;

        template<typename MSG>
        inline logger(MSG name)
        {
            _logger = Logger::getInstance(name);
        }
    };

}

#endif // LOG_HPP_INCLUDED
