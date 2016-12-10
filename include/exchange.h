#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <memory>
#include <functional>
#include <thread>
#include "../include/exchange_utils.h"
#include "../include/log.h"

class Exchange {
  public:
    Exchange(std::shared_ptr<Log> log, std::shared_ptr<Config> config) :
      reconnect(false),
      config(config),
      log(log),
      check_pings(false),
      pong(false) { }

    virtual void start() = 0;
    virtual void subscribe_to_ticker() = 0;
    virtual void subscribe_to_OHLC(std::string period) = 0;
    virtual void start_checking_pings() = 0;
    virtual void market_buy(double) = 0;
    virtual void market_sell(double) = 0;
    virtual void limit_buy(double, double) = 0;
    virtual void limit_sell(double, double) = 0;
    virtual void cancel_order(std::string) = 0;
    virtual void orderinfo(std::string) = 0;
    virtual void userinfo() = 0;
    virtual std::string status() = 0;

    // SETTERS FOR
    // SEMANTIC CALLBACKS FOR THE USER
    void set_ticker_callback(std::function<void(long, double, double, double)> callback) {
      ticker_callback = callback;
    }
    void set_OHLC_callback(std::function<void(std::string, long, double, double, double, double, double)> callback) {
      OHLC_callback = callback;
    }
    void set_open_callback(std::function<void()> callback) {
      open_callback = callback;
    }
    void set_trade_callback(std::function<void(std::string)> callback) {
      trade_callback = callback;
    }
    void set_orderinfo_callback(std::function<void(OrderInfo)> callback) {
      orderinfo_callback = callback;
    }
    void set_userinfo_callback(std::function<void(double, double)> callback) {
      userinfo_callback = callback;
    }
    void set_filled_callback(std::function<void(double)> callback) {
      filled_callback = callback;
    }

    // timestamp representing time of last received message
    std::chrono::nanoseconds ts_since_last;

    // when true, letting handler know to make a new Exchange object
    bool reconnect;
  protected:
    std::shared_ptr<Config> config;
    std::shared_ptr<Log> log;

    // SEMANTIC CALLBACKS FOR THE USER
    std::function<void(long, double, double, double)> ticker_callback;
    std::function<void(std::string, long, double, double, double, double, double)> OHLC_callback;
    std::function<void()> open_callback;
    std::function<void(std::string)> trade_callback;
    std::function<void(OrderInfo)> orderinfo_callback;
    std::function<void(double, double)> userinfo_callback;
    std::function<void(double)> filled_callback;

    // DATA AND FUNCTIONS RELATED TO PING/PONG
    std::shared_ptr<std::thread> ping_checker;
    bool check_pings;
    bool pong;
};
