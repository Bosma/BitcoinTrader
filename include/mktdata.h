#pragma once

#include <chrono>

#include <boost/circular_buffer.hpp>

#include "../include/strategies.h"

class MktData {
public:
  MktData(std::chrono::minutes, unsigned long);

  void add(OHLC);
  void add(const Ticker&);

  void set_new_bar_callback(std::function<void(OHLC)> callback) {
    new_bar_callback = callback;
  }

  void add_strategy(std::shared_ptr<Strategy>);

  std::string status();

  std::string strategies_status();

  buffer bars;
  // bar period in minutes
  std::chrono::minutes period;
private:
  std::vector<std::shared_ptr<Strategy>> strategies;
  std::function<void(OHLC)> new_bar_callback;
  std::mutex lock;
};
