#pragma once

#include <fstream>

extern std::ofstream& get_logger();
extern void write_log(std::string msg);