#include "../include/strategies.h"

using std::chrono::minutes; using std::string;
using std::function;        using std::shared_ptr;
using std::make_shared;

SMACrossover::SMACrossover(string name,
    function<void()> lc,
    function<void()> sc) :
  Strategy(name,
      minutes(15),
      {
        make_shared<MovingAverage>("sma_fast", 30),
        make_shared<MovingAverage>("sma_slow", 150)
      },
      lc, sc),
  crossed_above(false),
  crossed_below(false) { }

void SMACrossover::apply(OHLC bar) {
  if (bar.indis[name]["sma_fast"]["mavg"] > bar.indis[name]["sma_slow"]["mavg"] &&
      !crossed_above) {
    long_cb();

    crossed_above = true;
    crossed_below = false;
  }
  else if (bar.indis[name]["sma_fast"]["mavg"] < bar.indis[name]["sma_slow"]["mavg"] &&
      !crossed_below) {
    short_cb();

    crossed_below = true;
    crossed_above = false;
  }
}
