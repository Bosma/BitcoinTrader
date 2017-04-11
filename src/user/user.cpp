#include "handlers/bitcointrader.h"

// include each strategy you are going to use
#include "user/strategies/smacrossover.h"
#include "user/strategies/bbands.h"

using std::make_shared;
using std::shared_ptr;

void BitcoinTrader::user_specifications() {
  // create the exchanges that you will use
  okcoin_futs_h = make_shared<OKCoinFutsHandler>("OKCoinFuts",
                                                 config,
                                                 "futs_exchange_log", "futs_trading_log",
                                                 OKCoinFuts::ContractType::Weekly);

  // required to explicitly add each exchange to the handlers list
  exchange_handlers.push_back(okcoin_futs_h);

  // create and add strategies to each exchange
  okcoin_futs_h->signal_strategies.push_back(make_shared<SMACrossover>("SMACrossover", 1, okcoin_futs_h->trading_log));
}

// return a number between 1 and -1
double BitcoinTrader::blend_signals(shared_ptr<ExchangeHandler> handler) {
  // average the signals
  double signal = accumulate(handler->signal_strategies.begin(), handler->signal_strategies.end(), 0.0,
                                 [](double a, shared_ptr<SignalStrategy> b) -> double { return a + b->signal.get(); });

  if (signal > 1) signal = 1;
  else if (signal < -1) signal = -1;

  return signal;
}
