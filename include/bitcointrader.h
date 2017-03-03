#pragma once

#include "../include/log.h"
#include "../include/okcoin_futs.h"
#include "../include/okcoin_spot.h"
#include "../include/strategies.h"
#include "../include/mktdata.h"
#include "../include/config.h"
#include "../include/okcoin_futs_handler.h"
#include "../include/okcoin_spot_handler.h"

class BitcoinTrader {
public:
  // Exchanges and their data
  // ExchangeMeta includes data common to all exchanges
  // and use exchanges() to get a collection of them
  BitcoinTrader(std::shared_ptr<Config>);
  ~BitcoinTrader();

  std::function<void(std::string)> execution_callback;

  std::string status();
  void print_bars() {
    for (auto& m : okcoin_futs_h->mktdata) {
      std::string file_name = "okcoin_futs_" + std::to_string(m.first.count()) + "m.csv";
      std::ofstream csv(file_name);
      for (auto& bar : *m.second->bars)
        csv << bar.to_string() << std::endl;
      csv.close();
      std::cout << "written " << file_name << std::endl;
    }
  }

  // start the exchange
  // after setting callbacks and creating log files
  void start();

protected:
  std::shared_ptr<OKCoinFutsHandler> okcoin_futs_h;
  std::shared_ptr<OKCoinSpotHandler> okcoin_spot_h;

  // private config options
  // sourced from config file
  std::shared_ptr<Config> config;

  // used for logging trading actions
  std::shared_ptr<Log> trading_log;

  // if this is being destructed
  std::atomic<bool> done;

  // vector of threads performing some recurring actions
  // used for destructor
  std::vector<std::shared_ptr<std::thread>> running_threads;

  // used to check if the exchange is working
  void check_connection();

  // map of OHLC bars together with indicator values
  // keyed by period in minutes they represent

  std::vector<std::shared_ptr<Strategy>> strategies;

  // thread loop calling below two functions
  void position_management();
  // blend the signals from each strategy
  // according to some signal blending method
  double blend_signals();
  // match the signal with the exposure on the exchange
  void manage_positions(double);

  // EXECUTION ALGORITHMS
  // functions to set trade and orderinfo callbacks

  // generic market buy / sell amount of BTC
  bool market(Position, double);

  // market buy / sell (used for okcoin_futs)
  bool futs_market(OKCoinFuts::OrderType, double, int, std::chrono::seconds);

  // limit order that will cancel after some seconds
  // and after those seconds will run callback given
  // (to set take-profits / stop-losses)
  bool limit(Position, double, double, std::chrono::seconds);

  // good-til-cancelled limit orders
  // will run callback after receiving order_id
  bool GTC(Position, double, double);
  std::function<void(std::string)> GTC_callback;

  std::vector<std::shared_ptr<ExchangeHandler>> exchange_metas();
  std::vector<std::shared_ptr<Exchange>> exchanges() {
    std::vector<std::shared_ptr<Exchange>> to_return;
    for (auto x : exchange_metas())
      to_return.push_back(x->exchange);
    return to_return;
  }
};
