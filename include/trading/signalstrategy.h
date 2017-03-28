#pragma once

#include "trading/strategy.h"
#include "trading/trading.h"

class SignalStrategy : public Strategy {
public:
  SignalStrategy(std::string name,
                 double weight,
                 std::chrono::minutes period,
                 std::vector<std::shared_ptr<Indicator>> indicators,
                 std::shared_ptr<Log> log) :
      Strategy(name, weight, log),
      period(period),
      indicators(indicators) {}

  // TODO: implement long and short methods

  virtual void apply(const OHLC&) = 0;
  virtual void apply(const Ticker&) = 0;

  std::chrono::minutes period;
  std::vector<std::shared_ptr<Indicator>> indicators;
  Atomic<double> signal;
  Atomic<double> stop;

  std::string status() override;

  void process_stop(const Ticker&);
  void process_stop(const OHLC&);
};