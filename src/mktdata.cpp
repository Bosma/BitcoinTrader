#include "../include/mktdata.h"

MktData::MktData(std::chrono::minutes period, unsigned long size) :
    bars(size),
    period(period) {

}

void MktData::add(OHLC new_bar) {
  std::lock_guard<std::mutex> l(lock);

  // don't add bars of the same timestamp
  bool found = false;
  for (auto& x : bars)
    if (x.timestamp == new_bar.timestamp)
      found = true;

  if (bars.empty() || !found) {
    // if we receive a bar that's not contiguous
    // our old data is useless
    if (!bars.empty() &&
        new_bar.timestamp != bars.back().timestamp + period)
      bars.clear();

    // for each indicator
    // calculate the indicator value
    // from the bars (with the new value)
    for (auto strategy : strategies) {
      for (auto indicator : strategy->indicators)
        new_bar.indis[strategy->name][indicator->name] = indicator->calculate(bars);
      strategy->apply(new_bar);
    }

    bars.push_back(new_bar);
  }
}

void MktData::add(const Ticker& new_tick) {
  std::lock_guard<std::mutex> l(lock);

  for (auto &s : strategies)
    s->apply(new_tick);
}

void MktData::add_strategy(std::shared_ptr<Strategy> new_strategy) {
  strategies.push_back(new_strategy);
}

std::string MktData::status() {
  std::ostringstream os;
  os << "MktData with " << period.count() << "m period";
  os << ", size: " << bars.size();
  os << ", last OHLC bar: " + bars.back().to_string();
  return os.str();
}

std::string MktData::strategies_status() {
  std::string s = std::accumulate(std::next(strategies.begin()),
                                  strategies.end(),
                                  strategies[0]->status(),
                                  [](std::string a, std::shared_ptr<Strategy> s) {
                                    return a + ", " + s->status();
                                  });
  return s;
}