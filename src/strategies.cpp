#include "../include/strategies.h"

SMACrossover::SMACrossover(std::string name,
    std::function<void()> lc,
    std::function<void()> sc) :
  Strategy(name, lc, sc),
  sma_a(new MovingAverage("sma_a", 30)),
  sma_b(new MovingAverage("sma_b", 150)),
  crossed_above(false),
  crossed_below(false) { }

void SMACrossover::apply(std::shared_ptr<OHLC> bar, Ticker tick) {
  if (bar->indis["sma_a"]["mavg"] > bar->indis["sma_b"]["mavg"] &&
      !crossed_above) {
    long_cb();

    crossed_above = true;
    crossed_below = false;
  }
  else if (bar->indis["sma_a"]["mavg"] < bar->indis["sma_b"]["mavg"] &&
      !crossed_below) {
    short_cb();

    crossed_below = true;
    crossed_above = false;
  }
}
