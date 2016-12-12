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
        std::function<void()> lc,
        std::function<void()> sc) :
      name(name),
      period(period),
      indicators(indicators),
      long_cb(lc), short_cb(sc) { }

    virtual void apply(std::shared_ptr<OHLC>) = 0;

    std::string name;
    std::map<std::string, double> parameters;
    std::chrono::minutes period;
    std::vector<std::shared_ptr<Indicator>> indicators;

  protected:
    std::function<void()> long_cb;
    std::function<void()> short_cb;
};

class SMACrossover : public Strategy {
  public:
    SMACrossover(std::string,
        std::function<void()>,
        std::function<void()>);

    void apply(std::shared_ptr<OHLC> bar);

  private:
    bool crossed_above;
    bool crossed_below;
};
