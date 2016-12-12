#pragma once

#include <chrono>

#include <boost/circular_buffer.hpp>

#include "../include/trading.h"

class MktData {
  public:
    MktData(std::chrono::minutes period) :
      bars(new boost::circular_buffer<std::shared_ptr<OHLC>>(50)),
      period(period),
      indicators() { }

    void add(std::shared_ptr<OHLC> bar) {
      // don't add bars of the same timestamp
      bool found = false;
      for (auto x : *bars)
        if (x->timestamp == bar->timestamp)
          found = true;

      if (bars->empty() || !found) {
        // if we receive a bar that's not contiguous
        // our old data is useless
        if (!bars->empty() &&
            bar->timestamp != bars->back()->timestamp + (period.count() * 60000))
          bars->clear();

        bars->push_back(bar);

        // for each indicator
        // calculate the indicator value
        // from the bars (with the new value)
        for (auto &indi : indicators)
          bar->indis[indi->name] = indi->calculate(bars);

        for (auto strategy : strategies)
          strategy->apply(bar);
      }
    }

    void set_new_bar_callback(std::function<void(std::shared_ptr<OHLC>)> callback) {
      new_bar_callback = callback;
    }

    void add_strategy(std::shared_ptr<Strategy> to_add) {
      strategies.push_back(to_add);
    }

    void add_indicators(std::vector<std::shared_ptr<Indicator>> to_add) {
      indicators.insert(indicators.end(), to_add.begin(), to_add.end());
    }

    std::shared_ptr<boost::circular_buffer<std::shared_ptr<OHLC>>> bars;
    // bar period in minutes
    std::chrono::minutes period;
  private:
    std::vector<std::shared_ptr<Indicator>> indicators;
    std::vector<std::shared_ptr<Strategy>> strategies;
    std::function<void(std::shared_ptr<OHLC> bar)> new_bar_callback;
};
