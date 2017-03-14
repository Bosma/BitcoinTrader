#include "handlers/bitcointrader.h"

// include each strategy you are going to use
#include "user/strategies/smacrossover.h"

using std::make_shared;
using std::shared_ptr;

void BitcoinTrader::user_specifications() {
  // create the exchanges that you will use
  okcoin_futs_h = make_shared<OKCoinFutsHandler>("OKCoinFuts",
                                                 config,
                                                 "okcoin_futs_log",
                                                 "trading_log",
                                                 OKCoinFuts::ContractType::Weekly);

  // create and add strategies to each exchange
  okcoin_futs_h->strategies.push_back(make_shared<SMACrossover>("SMACrossover", okcoin_futs_h->trading_log));
}

double BitcoinTrader::blend_signals(shared_ptr<ExchangeHandler> handler) {
  // average the signals
  double signal_sum = accumulate(handler->strategies.begin(), handler->strategies.end(), 0,
                                 [](double a, shared_ptr<Strategy> b) { return a + b->signal.get(); });
  return signal_sum / handler->strategies.size();
}
