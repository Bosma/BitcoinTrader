#pragma once

#include "../include/log.h"
#include "../include/okcoin.h"
#include "../include/strategies.h"
#include "../include/config.h"

using stops_t = std::vector<std::shared_ptr<Stop>>;

class BitcoinTrader {
public:
  BitcoinTrader(std::shared_ptr<Config>);
  ~BitcoinTrader();

  // interactive commands
  void reconnect() { exchange->reconnect = true; }
  std::string status() { return exchange->status(); }
  std::string last(int);

  // interfaces to Exchange
  // (declared public because may be used interactively)
  void buy(double);
  void sell(double);
  void sell_all();
  void limit_buy(double, double);
  void limit_sell(double, double);
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
  std::shared_ptr<std::thread> connection_checker;
  void check_connection();

  // used to continually fetch user information
  // currently just btc and cny
  void fetch_userinfo();
  std::shared_ptr<std::thread> userinfo_fetcher;
  double user_btc;
  double user_cny;

  // updated in real time to latest tick
  Ticker tick;

  // contains OHLC bars together with indicator values
  int period;
  MktData mktdata;

  // held until trade and orderinfo callbacks are complete
  // to order market events
  std::mutex order_lock;

  // currently one strategy per OKCHandler
  std::unique_ptr<Strategy> strategy;

  // parameters used by the strategy and rules (tp/sl distance etc.)
  std::map<std::string, double> parameters;

  // live stops
  stops_t stops;
  // used so that the price for take profit limits is calculated
  // at the same time (and near the same code) as stops
  double tp_limit;
  // stops waiting to be added (ie, limit order waiting to be filled)
  stops_t pending_stops;

  // called every tick
  void handle_stops();

  // bools used to order subscribing to websocket channels in order
  bool received_userinfo;
  bool received_a_tick;

  // set up the exchange callbacks
  void setup_exchange_callbacks();

  // keep track of "fiat", "filling", "btc"
  // used currently instead of max positions
  std::string position;

  // functions to set trade and orderinfo callbacks
  // that lock and unlock order_lock
  void set_takeprofit_callbacks();
  void set_limit_callbacks(std::chrono::seconds);

  // functions and data relating to limit execution
  // currently will hold a limit for some seconds then cancel
  // it if not filled in time
  std::shared_ptr<std::thread> limit_checker;
  std::string current_limit;
  bool done_limit_check;
  double filled_amount;
};
