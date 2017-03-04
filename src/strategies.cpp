#include "../include/strategies.h"

using std::chrono::minutes; using std::string;
using std::function;        using std::shared_ptr;
using std::make_shared;     using std::to_string;

std::string Strategy::status() {
  std::ostringstream os;
  os << name;
  if (signal.has_been_set())
    os << " signal: " << signal.get() << " ";
  if (stop.has_been_set())
    os << " stop: " << stop.get() << " ";
  return os.str();
}

void Strategy::process_stop(const Ticker& new_tick) {
  if (stop.has_been_set()) {
    auto current_signal = signal.get();
    auto current_stop = stop.get();
    // if we are long and the bid is less than the stop price
    if ((current_signal > 0 && new_tick.bid < current_stop) ||
        // or if we are short and the ask is greater than the stop price
        (current_signal < 0 && new_tick.ask > current_stop)) {
      // set this signal to 0
      log->output(name + ": STOPPED OUT AT " + std::to_string(current_stop) + ", SIGNAL CHANGED FROM " +
                  std::to_string(current_signal) + " TO 0");
      signal.set(0);
    }
  }
}

void Strategy::process_stop(const OHLC& bar) {
  Ticker fake_tick;
  // since the bid is used if we are long, set the bid to the low of the bar to see if that bar would have stopped us out
  fake_tick.bid = bar.low;
  // vice versa
  fake_tick.ask = bar.high;
  process_stop(fake_tick);
}

SMACrossover::SMACrossover(string name, shared_ptr<Log> log) :
    Strategy(name,
             minutes(15),
             {
                 make_shared<MovingAverage>("sma_fast", 30),
                 make_shared<MovingAverage>("sma_slow", 150)
             },
             log),
    crossed_above(false),
    crossed_below(false) { }

void SMACrossover::apply(OHLC bar) {
  // used for backfilling
  process_stop(bar);

  // only send signals when we have all indicators set
  if (!bar.all_indis_set(name))
    return;

  double stop_percentage = 0.01;

  if (bar.indis[name]["sma_fast"]["mavg"] > bar.indis[name]["sma_slow"]["mavg"] &&
      !crossed_above) {
    auto new_stop = bar.close * (1 - stop_percentage);

    stop.set(new_stop);
    signal.set(1);

    log->output(name + ": LONGING WITH SIGNAL 1 AND STOP " + to_string(new_stop));

    crossed_above = true;
    crossed_below = false;
  }
  else if (bar.indis[name]["sma_fast"]["mavg"] < bar.indis[name]["sma_slow"]["mavg"] &&
           !crossed_below) {
    auto new_stop = bar.close * (1 + stop_percentage);
    stop.set(new_stop);
    signal.set(-1);

    log->output(name + ": SHORTING WITH SIGNAL -1 AND STOP " + to_string(new_stop));

    crossed_below = true;
    crossed_above = false;
  }
}

void SMACrossover::apply(const Ticker& new_tick) {
  process_stop(new_tick);
}
