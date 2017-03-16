#pragma once

#include "trading/signalstrategy.h"

class SMACrossover : public SignalStrategy {
public:
  SMACrossover(std::string, std::shared_ptr<Log>);

  void apply(const OHLC&);
  void apply(const Ticker&);

private:
  bool crossed_above;
  bool crossed_below;
};