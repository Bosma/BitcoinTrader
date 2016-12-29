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
  std::map<std::string, std::shared_ptr<std::thread>> running_threads;

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

  // EXECUTION ALGORITHMS
  // functions to set trade and orderinfo callbacks
  // that lock and unlock execution_lock
  
  // borrow amount and currency
  bool borrow(Currency, double);
  
  // go margin long
  void margin_long(double);
  void margin_short(double);

  // sell all BTC and repay all CNY
  void close_margin_long();
  // buy all BTC and repay all BTC
  void close_margin_short();
  
  // generic market buy / sell amount of BTC
  void market_buy(double);
  void market_sell(double);
  void set_market_callback(std::function<void(double, double, long)> cb,
      std::chrono::seconds timeout = std::chrono::seconds(10)) {
    if (!market_lock.try_lock_for(timeout)) {
      exchange_log->output("market callback not fired in time. Allowing new callback setter access.");
      market_callback(0, 0, 0);
      market_lock.lock();
    }
    market_callback_original = cb;
    market_callback = [&](double a, double b, long c) {
      market_callback_original(a, b, c);
      market_callback = nullptr;
      market_lock.unlock();
    };
  }
  std::timed_mutex market_lock;
  std::function<void(double, double, long)> market_callback;
  std::function<void(double, double, long)> market_callback_original;

  // limit order that will cancel after some seconds
  // and after those seconds will run callback given
  // (to set take-profits / stop-losses)
  void limit_buy(double, double, std::chrono::seconds);
  void limit_sell(double, double, std::chrono::seconds);
  void limit_algorithm(std::chrono::seconds);
  void set_limit_callback(std::function<void(double)> cb,
      std::chrono::seconds timeout = std::chrono::seconds(10)) {
    if (!limit_lock.try_lock_for(timeout)) {
      exchange_log->output("limit callback not fired in time. Allowing new callback setter access.");
      limit_callback(0);
      limit_lock.lock();
    }
    limit_callback_original = cb;
    limit_callback = [&](double a) {
      limit_callback_original(a);
      limit_callback = nullptr;
      limit_lock.unlock();
    };
  }
  std::timed_mutex limit_lock;
  std::function<void(double)> limit_callback;
  std::function<void(double)> limit_callback_original;

  // good-til-cancelled limit orders
  // will run callback after receiving order_id
  void GTC_buy(double, double);
  void GTC_sell(double, double);
  void set_GTC_callback(std::function<void(std::string)> cb,
      std::chrono::seconds timeout = std::chrono::seconds(5)) {
    if (!GTC_lock.try_lock_for(timeout)) {
      exchange_log->output("GTC callback not fired in time. Allowing new callback setter access.");
      GTC_callback(0);
      GTC_lock.lock();
    }
    GTC_callback_original = cb;
    GTC_callback = [&](std::string a) {
      GTC_callback_original(a);
      GTC_callback = nullptr;
      GTC_lock.unlock();
    };
  }
  std::timed_mutex GTC_lock;
  std::function<void(std::string)> GTC_callback;
  std::function<void(std::string)> GTC_callback_original;

  // functions and data relating to limit execution
  // currently will hold a limit for some seconds then cancel
  // it if not filled in time
  std::string current_limit;
  bool done_limit_check;
  double filled_amount;
};
