#pragma once

#include <chrono>

#include <boost/circular_buffer.hpp>

#include "../include/strategies.h"

class MktData {
public:
  MktData(std::chrono::minutes period) :
      bars(new boost::circular_buffer<OHLC>(50)),
      period(period) { }

  void add(OHLC bar) {
    std::lock_guard<std::mutex> l(lock);

    // don't add bars of the same timestamp
    bool found = false;
    for (auto x : *bars)
      if (x.timestamp == bar.timestamp)
        found = true;

    if (bars->empty() || !found) {
      // if we receive a bar that's not contiguous
      // our old data is useless
      if (!bars->empty() &&
          bar.timestamp != bars->back().timestamp + period)
        bars->clear();

      bars->push_back(bar);

      // for each indicator
      // calculate the indicator value
      // from the bars (with the new value)
      for (auto strategy : strategies) {
        for (auto indicator : strategy->indicators)
          bar.indis[strategy->name][indicator->name] = indicator->calculate(bars);
        strategy->apply(bar);
      }
    }
  }


  void add(Ticker tick) {
    for (auto &s : strategies) {
      s->apply(tick);
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

  std::string status() {
    std::ostringstream os;
    os << "MktData with " << period.count() << "m period";
    os << ", size: " << bars->size();
    os << ", last OHLC bar: " << bars->back().to_string();
    return os.str();
  }

  std::string strategies_status() {
    std::string s = std::accumulate(std::next(strategies.begin()),
                                    strategies.end(),
                                    strategies[0]->status(),
                                    [](std::string a, std::shared_ptr<Strategy> s) {
                                      return a + ", " + s->status();
                                    });
    return s;
  }

  std::shared_ptr<boost::circular_buffer<OHLC>> bars;
  // bar period in minutes
  std::chrono::minutes period;
private:
  std::vector<std::shared_ptr<Strategy>> strategies;
  std::function<void(OHLC)> new_bar_callback;
  std::mutex lock;
};
