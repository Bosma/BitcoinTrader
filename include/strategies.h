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

  virtual void apply(OHLC) = 0;
  virtual void apply(const Ticker) = 0;

  std::string name;
  std::chrono::minutes period;
  std::vector<std::shared_ptr<Indicator>> indicators;
  std::shared_ptr<Log> log;
  Atomic<double> signal;
  Atomic<double> stop;

  int max_lookback() {
    int max = 0;
    for (auto indicator : indicators)
      if (indicator->period > max)
        max = indicator->period;
    return max;
  }

  std::string status() {
    std::ostringstream os;
    os << name;
    if (signal.has_been_set())
      os << " signal: " << signal.get() << " ";
    if (stop.has_been_set())
      os << " stop: " << stop.get() << " ";
    return os.str();
  }

  void process_stop(const Ticker tick) {
    if (stop.has_been_set()) {
      auto current_signal = signal.get();
      auto current_stop = stop.get();
      // if we are long and the bid is less than the stop price
      if ((current_signal > 0 && tick.bid < current_stop) ||
          // or if we are short and the ask is greater than the stop price
          (current_signal < 0 && tick.ask > current_stop)) {
        // set this signal to 0
        log->output(name + ": STOPPED OUT AT " + std::to_string(current_stop) + ", SIGNAL CHANGED FROM " + std::to_string(current_signal) + " TO 0");
        signal.set(0);
      }
    }
  }
};

class SMACrossover : public Strategy {
public:
  SMACrossover(std::string, std::shared_ptr<Log>);

  void apply(OHLC bar);
  void apply(const Ticker);

private:
  bool crossed_above;
  bool crossed_below;
};
