#include "handlers/bitcointrader.h"

// include each strategy you are going to use
#include "user/strategies/smacrossover.h"

using std::make_shared;
using std::shared_ptr;

void BitcoinTrader::user_specifications() {
  // specify which exchange will manage positions
  // we're using OKCoin Futs for our basket of strategies
  strategy_h = okcoin_futs_h;

  // create and add strategies
  strategies.push_back(make_shared<SMACrossover>("SMACrossover", strategy_h->trading_log));
}

double BitcoinTrader::blend_signals() {
  // average the signals
  double signal_sum = accumulate(strategies.begin(), strategies.end(), 0,
                                 [](double a, shared_ptr<Strategy> b) { return a + b->signal.get(); });
  return signal_sum / strategies.size();
}
