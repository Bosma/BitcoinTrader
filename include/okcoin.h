#pragma once

#include <string>
#include <sstream>
#include <map>
#include <chrono>
#include <memory>
#include <fstream>

#include <openssl/md5.h>

#include "../json/json.hpp"

#include "../include/websocket.h"
#include "../include/log.h"
#include "../include/exchange_utils.h"
#include "../include/exchange.h"

class OKCoin : public Exchange {
  public:
    OKCoin(std::shared_ptr<Log> log, std::shared_ptr<Config> config);
    ~OKCoin();
    
    // start the websocket
    void start();

    // SEMANTIC COMMANDS FOR THE USER
    // most of these will have an associated callback
    // subscribe to trades feed
    void subscribe_to_ticker();
    // subscribe to OHLC bars
    // value of period is: 1min, 3min, 5min, 15min, 30min, 1hour, 2hour, 4hour, 6hour, 12hour, day, 3day, week
    void subscribe_to_OHLC(std::chrono::minutes);
    // market buy amount of USD
    void market_buy(double);
    // market sell amount of BTC
    void market_sell(double);
    // limit buy amount of BTC at price
    void limit_buy(double, double);
    // limit sell amount of BTC at price
    void limit_sell(double, double);
    // check a limit for some seconds, call filled callback
    // cancel limit order
    void cancel_order(std::string);
    // get order information by order_id
    void orderinfo(std::string);
    // get user info
    void userinfo();
    // borrow percent of possible currency
    BorrowInfo borrow(Currency, double);
    // find out how much is borrowed, close, pay back
    double close_borrow(Currency);
    // send ping to OKCoin
    void ping();
    // backfill OHLC period
    void backfill_OHLC(std::chrono::minutes, int);
    // return a string of each channels name and last received message
    std::string status();

  private:
    const std::string OKCOIN_URL = "wss://real.okcoin.com:10440/websocket/okcoinapi";
    std::string api_key;
    std::string secret_key;
    websocket ws;
    
    // INTERNAL REST COMMANDS
    // get lend depth
    json lend_depth(Currency);
    // get user borrow information
    json borrows_info(Currency);
    // request borrow
    json borrow_money(Currency, double, double, int);
    // debt list
    json unrepayments_info(Currency);
    // pay off debt
    json repayment(std::string);
    
    // WEBSOCKET CALLBACKS
    void on_message(std::string const &);
    void on_open();
    void on_close();
    void on_fail();
    void on_error(std::string const &);

    // DATA AND FUNCTIONS USED BY THE SEMANTIC COMMANDS
    std::map<std::string, std::shared_ptr<Channel>> channels;
    void subscribe_to_channel(std::string const &);
    void unsubscribe_to_channel(std::string const &);
    void order(std::string, std::string, std::string price = "");

    // GET MD5 HASH OF PARAMETERS
    std::string sign(std::string);

    std::string period_conversions(std::chrono::minutes period) {
      if (period == std::chrono::minutes(1))
        return "1min";
      if (period == std::chrono::minutes(15))
        return "15min";
      else
        return "";
    }
    std::chrono::minutes period_conversions(std::string okcoin_kline) {
      if (okcoin_kline == "1min")
        return std::chrono::minutes(1);
      if (okcoin_kline == "15min")
        return std::chrono::minutes(15);
      else
        return std::chrono::minutes(0);
    }
    
    // OKCOIN ERROR CODES
    std::map<std::string, std::string> error_reasons;
    void populate_error_reasons();

    // HANDLERS FOR CHANNEL MESSAGES
    void OHLC_handler(std::string, nlohmann::json);
    void ticker_handler(nlohmann::json);
    void orderinfo_handler(nlohmann::json);
};
