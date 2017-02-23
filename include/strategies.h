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
        std::vector<std::shared_ptr<Indicator>> indicators) :
      name(name),
      period(period),
      indicators(indicators) { }

    virtual void apply(OHLC) = 0;

    std::string name;
    std::chrono::minutes period;
    std::vector<std::shared_ptr<Indicator>> indicators;
    Atomic<double> signal;
    Atomic<double> stop;
    Atomic<Ticker> tick;

    int max_lookback() {
      int max = 0;
      for (auto indicator : indicators)
        if (indicator->period > max)
          max = indicator->period;
      return max;
    }

    void apply(const Ticker new_tick) {
      tick.set(new_tick);

      auto current_signal = signal.get();
      auto current_stop = stop.get();
      // if we are long and the bid is less than the stop price
      if ((current_signal > 0 && new_tick.bid < current_stop) ||
          // or if we are short and the ask is greater than the stop price
          (current_signal < 0 && new_tick.ask > current_stop)) {
        // set this signal to 0
        // TODO: log this in the trading_log somehow
        signal.set(0);
      }
    }
};

class SMACrossover : public Strategy {
  public:
    SMACrossover(std::string);

    void apply(OHLC bar);

  private:
    bool crossed_above;
    bool crossed_below;
};
