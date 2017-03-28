#pragma once

#include "trading/signalstrategy.h"

class BBands : public SignalStrategy {
public:
  BBands(std::string, double weight, std::shared_ptr<Log>);

  void apply(const OHLC&);
  void apply(const Ticker&);

private:
  bool crossed_below;
  bool crossed_above;
};