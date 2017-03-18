#include "handlers/bitcointrader.h"

// include each strategy you are going to use
#include "user/strategies/smacrossover.h"

using std::make_shared;
using std::shared_ptr;

void BitcoinTrader::user_specifications() {
  // create the exchanges that you will use
  okcoin_futs_h = make_shared<OKCoinFutsHandler>("OKCoinFuts",
                                                 config,
                                                 OKCoinFuts::ContractType::Weekly);


  // create the log files
  okcoin_futs_h->make_log("trading", "futs_trading_log");
  okcoin_futs_h->make_log("execution", "futs_execution_log");
  okcoin_futs_h->make_log("exchange", "futs_exchange_log");

  // required to explicitly add each exchange to the handlers list
  exchange_handlers.push_back(okcoin_futs_h);

  // create and add strategies to each exchange
  okcoin_futs_h->signal_strategies.push_back(make_shared<SMACrossover>("SMACrossover",
                                                                       okcoin_futs_h->logs.at("trading")));
}

double BitcoinTrader::blend_signals(shared_ptr<ExchangeHandler> handler) {
  // average the signals
  double signal_sum = accumulate(handler->signal_strategies.begin(), handler->signal_strategies.end(), 0,
                                 [](double a, shared_ptr<SignalStrategy> b) { return a + b->signal.get(); });
  return signal_sum / handler->signal_strategies.size();
}
