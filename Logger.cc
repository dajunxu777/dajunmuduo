    #include "Logger.h"
    
    //获取日志唯一的实例对象
    Logger& Logger::getinstance(){
        static Logger logger;
        return logger;
    }
    //设置日志级别
    void Logger::setLogLevel(LogLevel level){
        logLevel_ = level;
    }
    //写日志
    void Logger::log(std::string msg){
        switch(logLevel_)
        {
            case INFO:
                std::cout << "[INFO]";
                break;
            case ERROR:
                std::cout << "[ERROR]";
                break;
            case FATAL:
                std::cout << "[FATAL]";
                break;
            case DEBUG:
                std::cout << "[DEBUG]";
                break;
            default:
                break;
        }
        // 打印时间和msg
        std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
    }