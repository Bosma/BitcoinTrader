#pragma once

#include "trading/strategies.h"

class SMACrossover : public Strategy {
public:
  SMACrossover(std::string, std::shared_ptr<Log>);

  void apply(const OHLC&);
  void apply(const Ticker&);

private:
  bool crossed_above;
  bool crossed_below;
};