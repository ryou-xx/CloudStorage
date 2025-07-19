#include "AsyncLogger.hpp"

int main()
{
    using namespace mylog;
    AsyncLogger logger(LogLevel::INFO);
    logger.debug("This is a debug message");
    logger.info("This is a info message");
    logger.warn("This is a warn message");
    logger.error("This is a error message");
    logger.fatal("This is a fatal message");
    logger.log(LogLevel::INFO, "This is a log func test message");
    return 0;
}