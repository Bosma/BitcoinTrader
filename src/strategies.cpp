#include "../include/strategies.h"

using std::chrono::minutes; using std::string;
using std::function;        using std::shared_ptr;
using std::make_shared;

SMACrossover::SMACrossover(string name) :
  Strategy(name,
      minutes(15),
      {
        make_shared<MovingAverage>("sma_fast", 30),
        make_shared<MovingAverage>("sma_slow", 150)
      }),
  crossed_above(false),
  crossed_below(false) { }

void SMACrossover::apply(OHLC bar) {
  auto current_tick = tick.get();
  double stop_percentage = 0.01;

  if (bar.indis[name]["sma_fast"]["mavg"] > bar.indis[name]["sma_slow"]["mavg"] &&
      !crossed_above) {
    stop.set(current_tick.bid * (1 - stop_percentage));

    signal.set(1);

    crossed_above = true;
    crossed_below = false;
  }
  else if (bar.indis[name]["sma_fast"]["mavg"] < bar.indis[name]["sma_slow"]["mavg"] &&
      !crossed_below) {
    stop.set(current_tick.bid * (1 + stop_percentage));

    signal.set(-1);

    crossed_below = true;
    crossed_above = false;
  }
}