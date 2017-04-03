#pragma once

#include <boost/optional.hpp>

#include "utilities/log.h"
#include "exchanges/okcoin_futs.h"
#include "exchanges/okcoin_spot.h"
#include "trading/mktdata.h"
#include "utilities/config.h"
#include "okcoin_futs_handler.h"
#include "okcoin_spot_handler.h"

class BitcoinTrader {
public:
  // Exchanges and their data
  // ExchangeMeta includes data common to all exchanges
  // and use exchanges() to get a collection of them
  BitcoinTrader(std::shared_ptr<Config>);
  ~BitcoinTrader();

  std::string status();
  void print_bars();

  // start the exchange
  // after setting callbacks and creating log files
  void start();

protected:
  std::shared_ptr<OKCoinFutsHandler> okcoin_futs_h;
  std::vector<std::shared_ptr<ExchangeHandler>> exchange_handlers;

  // user defined functions
  void user_specifications();
  double blend_signals(std::shared_ptr<ExchangeHandler>);

  // private config options
  // sourced from config file
  std::shared_ptr<Config> config;

  // if this is being destructed
  std::atomic<bool> done;

  // vector of threads performing some recurring actions
  // used for destructor
  std::vector<std::thread> running_threads;

  // used to check if the exchange is working
  void manage_connections();

  void position_management();

  std::vector<std::shared_ptr<Exchange>> exchanges() {
    std::vector<std::shared_ptr<Exchange>> to_return;
    for (auto x : exchange_handlers)
      to_return.push_back(x->exchange);
    return to_return;
  }
};
