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
        std::function<void()> lc,
        std::function<void()> sc) :
      name(name), long_cb(lc), short_cb(sc) { }

    virtual void apply(std::shared_ptr<OHLC>, Ticker) = 0;
    virtual std::vector<std::shared_ptr<Indicator>> get_indicators() = 0;

    std::string name;
  protected:
    std::function<void()> long_cb;
    std::function<void()> short_cb;
};

class SMACrossover : public Strategy {
  public:
    SMACrossover(std::string,
        std::function<void()>,
        std::function<void()>);

    void apply(std::shared_ptr<OHLC> bar, Ticker);

    std::vector<std::shared_ptr<Indicator>> get_indicators() {
      std::vector<std::shared_ptr<Indicator>> indis({sma_a, sma_b});
      return indis;
    }

  private:
    std::shared_ptr<MovingAverage> sma_a;
    std::shared_ptr<MovingAverage> sma_b;

    bool crossed_above;
    bool crossed_below;
};
