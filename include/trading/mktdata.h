#pragma once

#include <chrono>

#include <boost/circular_buffer.hpp>

#include "signalstrategy.h"

class MktData {
public:
  MktData(std::chrono::minutes, unsigned long, std::shared_ptr<Log>);

  void add(const OHLC&);
  void add(const Ticker&);

  void add_strategy(std::shared_ptr<SignalStrategy>);

  std::string status();

  buffer bars;
  // bar period in minutes
  std::chrono::minutes period;
private:
  std::vector<std::shared_ptr<SignalStrategy>> strategies;
  std::shared_ptr<Log> log;
  std::mutex lock;
};
