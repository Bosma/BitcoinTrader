#include "handlers/bitcointrader.h"

// include each strategy you are going to use
#include "user/strategies/smacrossover.h"

using std::make_shared;

void BitcoinTrader::create_strategies() {
  strategies.push_back(make_shared<SMACrossover>("SMACrossover", trading_log));
}