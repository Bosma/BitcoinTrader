#include "../include/strategies.h"

using std::chrono::minutes; using std::string;
using std::function;        using std::shared_ptr;
using std::make_shared;     using std::to_string;

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

void SMACrossover::apply(const Ticker new_tick) {
  process_stop(new_tick);
}