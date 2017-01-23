#pragma once

#include "../include/log.h"
#include "../include/okcoin_futs.h"
#include "../include/okcoin_spot.h"
#include "../include/strategies.h"
#include "../include/mktdata.h"
#include "../include/config.h"
#include "../include/exchange_data.h"

using stops_t = std::vector<std::shared_ptr<Stop>>;

class BitcoinTrader {
public:
  // Exchanges and their data
  // ExchangeMeta includes data common to all exchanges
  // and use exchanges() to get a collection of them
  BitcoinTrader(std::shared_ptr<Config>);
  ~BitcoinTrader();

  std::function<void(std::string)> execution_callback;

  // interactive commands
  void reconnect() {
    for (auto exchange : exchanges())
      exchange->reconnect = true;
  }
  std::string status();

  // interfaces to Exchange
  // (declared public because may be used interactively)
  void cancel_order(std::string);

  // start the exchange
  // after setting callbacks and creating log files
  void start();

protected:
  ExchangeData<OKCoinFuts> okcoin_futs;
  ExchangeData<OKCoinSpot> okcoin_spot;

  // private config options
  // sourced from config file
  std::shared_ptr<Config> config;

  // used for logging trading actions
  std::shared_ptr<Log> trading_log;

  // if this is being destructed
  bool done;

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

  // live stops
  stops_t stops;
  // stops waiting to be added (ie, limit order waiting to be filled)
  // EXECUTION ALGORITHMS
  // functions to set trade and orderinfo callbacks
  // that lock and unlock execution_lock
  
  // generic market buy / sell amount of BTC
  template <class T>
  void market(ExchangeData<T>, Position, double);
  std::function<void(double, double, std::string)> market_callback;

  // limit order that will cancel after some seconds
  // and after those seconds will run callback given
  // (to set take-profits / stop-losses)
  template <class T>
  void limit(ExchangeData<T>, Position, double, double, std::chrono::seconds);
  std::function<void(double)> limit_callback;

  // good-til-cancelled limit orders
  // will run callback after receiving order_id
  void GTC(ExchangeMeta, Position, double, double);
  std::function<void(std::string)> GTC_callback;

  stops_t pending_stops;

  // USERINFO FETCHING
  void fetch_userinfo();

  static std::string dir_to_string(Position direction) { return (direction == Position::Long) ? "LONG" : "SHORT"; }
  static std::string dir_to_action(Position direction) { return (direction == Position::Long) ? "BUY" : "SELL"; }
  static std::string dir_to_past_tense(Position direction) { return (direction == Position::Long) ? "BOUGHT" : "SOLD"; }
  static Currency dir_to_own(Position direction) { return (direction == Position::Long) ? Currency::BTC : Currency::USD; }
  static Currency dir_to_tx(Position direction) { return (direction == Position::Long) ? Currency::USD : Currency::BTC; }
  static std::string cur_to_string(Currency currency) { return (currency == Currency::BTC) ? "BTC" : "USD"; }

  std::vector<std::shared_ptr<ExchangeMeta>> exchange_metas();
  std::vector<std::shared_ptr<Exchange>> exchanges() {
    std::vector<std::shared_ptr<Exchange>> to_return;
    for (auto x : exchange_metas())
      to_return.push_back(x->exchange);
    return to_return;
  }
};
