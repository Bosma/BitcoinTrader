#pragma once

#include <string>
#include <iostream>
#include <functional>
#include <vector>
#include <memory>

#include "utilities/log.h"

class Strategy {
public:
  Strategy(std::string name, std::shared_ptr<Log> log) :
      name(name),
      log(log) { }

  virtual std::string status() = 0;

  std::string name;
  std::shared_ptr<Log> log;
};