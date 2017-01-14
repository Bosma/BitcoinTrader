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
#include "../include/okcoin.h"

class OKCoinFuts : public OKCoin {
  public:
    OKCoinFuts(std::shared_ptr<Log> log, std::shared_ptr<Config> config);
    
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
    // backfill OHLC period
    void backfill_OHLC(std::chrono::minutes, int);

  private:
    // WEBSOCKET CALLBACKS
    void on_message(std::string const &);

    void order(std::string, std::string, std::string price = "");

    // HANDLERS FOR CHANNEL MESSAGES
    void OHLC_handler(std::string, nlohmann::json);
    void ticker_handler(nlohmann::json);
    void orderinfo_handler(nlohmann::json);
};
