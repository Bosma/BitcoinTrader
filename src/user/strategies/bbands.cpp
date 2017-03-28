#include "user/strategies/bbands.h"

using std::shared_ptr;
using std::string;
using std::make_shared;
using std::to_string;
using namespace std::chrono_literals;

BBands::BBands(string name, double weight, shared_ptr<Log> log) :
    SignalStrategy(name,
                   weight,
                   1min,
                   {
                       make_shared<BollingerBands>("bbands")
                   },
                   log),
    crossed_below(false),
    crossed_above(false) {}

void BBands::apply(const OHLC& bar) {
  // used for backfilling
  process_stop(bar);

  // only send signals when we have all indicators set
  if (!bar.all_indis_set(name))
    return;

  double stop_percentage = 0.01;

  if (bar.close < bar.indis.at(name).at("bbands").at("dn").get() &&
      !crossed_below) {
    auto new_stop = bar.close * (1 - stop_percentage);

    stop.set(new_stop);
    signal.set(weight);

    log->output(name + ": LONGING WITH SIGNAL 1 AND STOP " + to_string(new_stop));

    crossed_below = true;
    crossed_above = false;
  }
  else if (bar.close > bar.indis.at(name).at("bbands").at("up").get() &&
           !crossed_above) {
    auto new_stop = bar.close * (1 + stop_percentage);
    stop.set(new_stop);
    signal.set(-weight);

    log->output(name + ": SHORTING WITH SIGNAL -1 AND STOP " + to_string(new_stop));

    crossed_above = true;
    crossed_below = false;
  }
}

void BBands::apply(const Ticker& new_tick) {
  process_stop(new_tick);
}

