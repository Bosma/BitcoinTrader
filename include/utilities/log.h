#pragma once

#include <iostream>
#include <ctime>
#include <sstream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <functional>
#include <memory>
#include <map>

#include "utilities/config.h"
#include "utilities/exchange_utils.h"

class Log {
public:
  Log(std::string file_name = "", std::shared_ptr<Config> = {});
  ~Log();

  void output(std::string, bool alert = false);
  void send_email(std::string);
private:
  std::shared_ptr<Config> config;
  std::ostream *log;
  std::ofstream log_file;
};
