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

    double signal;

    int max_lookback() {
      int max = 0;
      for (auto indicator : indicators)
        if (indicator->period > max)
          max = indicator->period;
      return max;
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
