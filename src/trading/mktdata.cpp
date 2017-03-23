#include "trading/mktdata.h"

MktData::MktData(std::chrono::minutes period, unsigned long size) :
    bars(size),
    period(period) {

}

void MktData::add(const OHLC& new_bar) {
  std::lock_guard<std::mutex> l(lock);
  OHLC bar = new_bar;

  // don't add bars of the same timestamp
  bool found = false;
  for (auto& x : bars)
    if (x.timestamp == bar.timestamp)
      found = true;

  // TODO: does reconnecting always clear?
  if (bars.empty() || !found) {
    // if we receive a bar that's not contiguous
    // our old data is useless
    if (!bars.empty() &&
        bar.timestamp != bars.back().timestamp + period)
      bars.clear();

    bars.push_back(std::move(bar));

    // for each indicator
    // calculate the indicator value
    // from the bars (with the new value)
    for (const auto& strategy : strategies) {
      for (const auto& indicator : strategy->indicators)
        bars.back().indis[strategy->name][indicator->name] = indicator->calculate(bars);
      strategy->apply(bars.back());
    }
  }
}

void MktData::add(const Ticker& new_tick) {
  std::lock_guard<std::mutex> l(lock);

  for (auto &s : strategies)
    s->apply(new_tick);
}

void MktData::add_strategy(std::shared_ptr<SignalStrategy> new_strategy) {
  strategies.push_back(new_strategy);
}

std::string MktData::status() {
  std::ostringstream os;
  os << "MktData with " << period.count() << "m period";
  os << ", size: " << bars.size();
  return os.str();
}