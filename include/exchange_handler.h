#pragma once

#include "../include/log.h"
#include "../include/okcoin.h"
#include "../include/strategies.h"
#include "../include/mktdata.h"
#include "../include/config.h"

using stops_t = std::vector<std::shared_ptr<Stop>>;

class BitcoinTrader {
public:
  BitcoinTrader(std::shared_ptr<Config>);
  ~BitcoinTrader();

  std::function<void(std::string)> execution_callback;

  // interactive commands
  void reconnect() { exchange->reconnect = true; }
  void call_long_cb() { strategies[0]->long_cb(); }
  void call_short_cb() { strategies[0]->short_cb(); }
  std::string status();

  // interfaces to Exchange
  // (declared public because may be used interactively)
  void cancel_order(std::string);

  // start the exchange
  // after setting callbacks and creating log files
  void start();

protected:
  // source of data and market actions
  // is dynamic because on reconnects old one is thrown out
  // and new one allocated
  std::shared_ptr<Exchange> exchange;

  // private config options
  // sourced from config file
  std::shared_ptr<Config> config;

  // used for logging trading actions
  std::shared_ptr<Log> trading_log;
  // used for logging exchange actions
  std::shared_ptr<Log> exchange_log;

  // if this is being destructed
  bool done;

  // vector of threads performing some recurring actions
  // used for destructor
  std::vector<std::shared_ptr<std::thread>> running_threads;

  // used to check if the exchange is working
  void check_connection();

  // updated in real time to latest tick
  Ticker tick;

  // map of OHLC bars together with indicator values
  // keyed by period in minutes they represent
  std::map<std::chrono::minutes, std::shared_ptr<MktData>> mktdata;

  std::vector<std::shared_ptr<Strategy>> strategies;

  // live stops
  stops_t stops;
  // used so that the price for take profit limits is calculated
  // at the same time (and near the same code) as stops
  double tp_limit;
  // stops waiting to be added (ie, limit order waiting to be filled)
  stops_t pending_stops;

  // called every tick
  void handle_stops();

  // bools used to order subscribing to websocket channels
  bool received_a_tick;

  // set up the exchange callbacks
  void setup_exchange_callbacks();

  // USERINFO FETCHING
  // so execution algos can check if their actions have resulted in changed
  // balance values
  bool fetching_userinfo_already;
  bool subscribe;
  void fetch_userinfo();
  Exchange::UserInfo userinfo;
  std::mutex userinfo_lock;
  void set_userinfo(Exchange::UserInfo new_info) {
    std::lock_guard<std::mutex> l(userinfo_lock);
    userinfo = new_info;
  }
  Exchange::UserInfo get_userinfo() {
    std::lock_guard<std::mutex> l(userinfo_lock);
    return userinfo;
  }

  // EXECUTION ALGORITHMS
  // functions to set trade and orderinfo callbacks
  // that lock and unlock execution_lock
  
  // borrow amount and currency
  Exchange::BorrowInfo borrow(Currency, double);
  
  void close_short_then_long(double);
  void close_long_then_short(double);

  void margin_long(double);
  void margin_short(double);
  
  // generic market buy / sell amount of BTC
  void market_buy(double);
  void market_sell(double);
  void set_market_callback(std::function<void(double, double, std::string)> cb) {
    market_callback = cb;
  }
  std::function<void(double, double, std::string)> market_callback;
  OrderInfo current_order;
  std::mutex current_order_lock;
  void set_current_order(OrderInfo new_info) {
    std::lock_guard<std::mutex> l(current_order_lock);
    current_order = new_info;
  }
  OrderInfo get_current_order() {
    std::lock_guard<std::mutex> l(current_order_lock);
    return current_order;
  }
  void clear_current_order() {
    std::lock_guard<std::mutex> l(current_order_lock);
    OrderInfo cleared;
    current_order = cleared;
  }

  // limit order that will cancel after some seconds
  // and after those seconds will run callback given
  // (to set take-profits / stop-losses)
  void limit_buy(double, double, std::chrono::seconds);
  void limit_sell(double, double, std::chrono::seconds);
  void limit_algorithm(std::chrono::seconds);
  void set_limit_callback(std::function<void(double)> cb) {
    limit_callback = cb;
  }
  std::function<void(double)> limit_callback;

  // good-til-cancelled limit orders
  // will run callback after receiving order_id
  void GTC_buy(double, double);
  void GTC_sell(double, double);
  void set_GTC_callback(std::function<void(std::string)> cb) {
    GTC_callback = cb;
  }
  std::function<void(std::string)> GTC_callback;
};
