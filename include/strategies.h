#pragma once

#include <string>
#include <iostream>
#include <functional>
#include <vector>
#include <memory>

#include "../include/trading.h"
#include "../include/log.h"

class Strategy {
public:
  Strategy(std::string name,
           std::chrono::minutes period,
           std::vector<std::shared_ptr<Indicator>> indicators,
           std::shared_ptr<Log> log) :
      name(name),
      period(period),
      indicators(indicators),
      log(log) { }

  virtual void apply(const OHLC&) = 0;
  virtual void apply(const Ticker&) = 0;

  std::string name;
  std::chrono::minutes period;
  std::vector<std::shared_ptr<Indicator>> indicators;
  std::shared_ptr<Log> log;
  Atomic<double> signal;
  Atomic<double> stop;

  std::string status();

  void process_stop(const Ticker&);
  void process_stop(const OHLC&);
};

class SMACrossover : public Strategy {
public:
  SMACrossover(std::string, std::shared_ptr<Log>);

  void apply(const OHLC&);
  void apply(const Ticker&);

private:
  bool crossed_above;
  bool crossed_below;
};
