#include "tape_scanner/utils/Logger.h"

std::ofstream& get_logger()
{
    static std::ofstream logger("logs.txt");

    return logger;
}

void write_log(std::string msg)
{
    get_logger() << msg << std::endl;
    get_logger().flush();
}