#pragma once

#include <chrono>

#include <boost/circular_buffer.hpp>

#include "../include/trading.h"

class MktData {
  public:
    MktData(std::chrono::minutes period) :
      bars(new boost::circular_buffer<OHLC>(50)),
      period(period) { }

    void add(OHLC bar, bool backfilling = false) {
      // don't add bars of the same timestamp
      bool found = false;
      for (auto x : *bars)
        if (x.timestamp == bar.timestamp)
          found = true;

      if (bars->empty() || !found) {
        // if we receive a bar that's not contiguous
        // our old data is useless
        if (!bars->empty() &&
            bar.timestamp != bars->back().timestamp + (period.count() * 60000))
          bars->clear();

        bars->push_back(bar);

        // for each indicator
        // calculate the indicator value
        // from the bars (with the new value)
        for (auto strategy : strategies) {
          for (auto indicator : strategy->indicators)
            bar.indis[strategy->name][indicator->name] = indicator->calculate(bars);
          // don't send signals on backfills
          if (!backfilling)
            strategy->apply(bar);
        }
      }
    }

    void set_new_bar_callback(std::function<void(OHLC)> callback) {
      new_bar_callback = callback;
    }

    void add_strategy(std::shared_ptr<Strategy> to_add) {
      if (to_add->max_lookback() > bars->capacity())
        bars->set_capacity(to_add->max_lookback());
      strategies.push_back(to_add);
    }

    std::shared_ptr<boost::circular_buffer<OHLC>> bars;
    // bar period in minutes
    std::chrono::minutes period;
  private:
    std::vector<std::shared_ptr<Strategy>> strategies;
    std::function<void(OHLC)> new_bar_callback;
};
