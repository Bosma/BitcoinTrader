#include "user/strategies/smacrossover.h"

using std::shared_ptr;
using std::string;
using std::make_shared;
using std::to_string;
using namespace std::chrono_literals;

SMACrossover::SMACrossover(string name, double weight, shared_ptr<Log> log) :
    SignalStrategy(name,
                   weight,
                   15min,
                   {
                       make_shared<MovingAverage>("sma_fast", 30),
                       make_shared<MovingAverage>("sma_slow", 150)
                   },
                   log),
    crossed_above(false),
    crossed_below(false) {}

void SMACrossover::apply(const OHLC& bar) {
  // used for backfilling
  process_stop(bar);

  // only send signals when we have all indicators set
  if (!bar.all_indis_set(name))
    return;

  double stop_percentage = 0.01;

  if (bar.indis.at(name).at("sma_fast").at("mavg").get() > bar.indis.at(name).at("sma_slow").at("mavg").get() &&
      !crossed_above) {
    auto new_stop = bar.close * (1 - stop_percentage);

    go_long(1, new_stop);

    crossed_above = true;
    crossed_below = false;
  }
  else if (bar.indis.at(name).at("sma_fast").at("mavg").get() < bar.indis.at(name).at("sma_slow").at("mavg").get() &&
           !crossed_below) {
    auto new_stop = bar.close * (1 + stop_percentage);

    go_short(1, new_stop);

    crossed_below = true;
    crossed_above = false;
  }
}

void SMACrossover::apply(const Ticker& new_tick) {
  process_stop(new_tick);
}

