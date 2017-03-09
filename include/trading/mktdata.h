#pragma once

#include <chrono>

#include <boost/circular_buffer.hpp>

#include "strategies.h"

class MktData {
public:
  MktData(std::chrono::minutes, unsigned long);

  void add(const OHLC&);
  void add(const Ticker&);

  void add_strategy(std::shared_ptr<Strategy>);

  std::string status();

  std::string strategies_status();

  buffer bars;
  // bar period in minutes
  std::chrono::minutes period;
private:
  std::vector<std::shared_ptr<Strategy>> strategies;
  std::mutex lock;
};
