#include "trading/signalstrategy.h"

using std::string;
using std::to_string;

std::string SignalStrategy::status() {
  std::ostringstream os;
  os << name << " (" << period.count() << "m)";
  if (signal.has_been_set())
    os << " signal: " << signal.get() << " ";
  if (stop.has_been_set())
    os << " stop: " << stop.get() << " ";
  return os.str();
}

void SignalStrategy::process_stop(const Ticker& new_tick) {
  if (stop.has_been_set()) {
    auto current_signal = signal.get();
    auto current_stop = stop.get();
    // if we are long and the bid is less than the stop price
    if ((current_signal > 0 && new_tick.bid < current_stop) ||
        // or if we are short and the ask is greater than the stop price
        (current_signal < 0 && new_tick.ask > current_stop)) {
      // set this signal to 0
      log->output(name + ": STOPPED OUT AT " + to_string(current_stop) + ", SIGNAL CHANGED FROM " +
                  to_string(current_signal) + " TO 0");
      signal.set(0);
      stop.clear();
    }
  }
}

void SignalStrategy::process_stop(const OHLC& bar) {
  Ticker fake_tick;
  // since the bid is used if we are long, set the bid to the low of the bar to see if that bar would have stopped us out
  fake_tick.bid = bar.low;
  // vice versa
  fake_tick.ask = bar.high;
  process_stop(fake_tick);
}