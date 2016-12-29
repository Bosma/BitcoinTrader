#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <functional>
#include <memory>
#include <functional>
#include <thread>
#include "../include/exchange_utils.h"
#include "../include/log.h"

using json = nlohmann::json;

class Exchange {
  public:
    Exchange(std::string name, std::shared_ptr<Log> log, std::shared_ptr<Config> config) :
      name(name),
      reconnect(false),
      config(config),
      log(log) { }

    virtual void start() = 0;
    virtual void subscribe_to_ticker() = 0;
    virtual void subscribe_to_OHLC(std::chrono::minutes) = 0;
    virtual void market_buy(double) = 0;
    virtual void market_sell(double) = 0;
    virtual void limit_buy(double, double) = 0;
    virtual void limit_sell(double, double) = 0;
    virtual void cancel_order(std::string) = 0;
    virtual void orderinfo(std::string) = 0;
    struct UserInfo { double asset_net; double free_btc; double free_cny; double borrow_btc; double borrow_cny; };
    virtual void userinfo() = 0;
    struct BorrowInfo { std::string id; double amount; double rate; };
    virtual BorrowInfo borrow(Currency, double = 1) = 0;
    virtual double close_borrow(Currency) = 0;

    virtual void ping() = 0;
    virtual void backfill_OHLC(std::chrono::minutes, int) = 0;
    virtual std::string status() = 0;

    // STREAMING CALLBACKS
    // "SET AND FORGET"
    void set_ticker_callback(std::function<void(long, double, double, double)> callback) {
      ticker_callback = callback;
    }
    void set_OHLC_callback(std::function<void(std::chrono::minutes, long, double, double, double, double, double, bool)> callback) {
      OHLC_callback = callback;
    }
    void set_open_callback(std::function<void()> callback, std::chrono::seconds timeout = std::chrono::seconds(10)) {
      open_callback = callback;
    }

    // ONE-OFF CALLBACKS
    // WILL HOLD LOCK UNTIL A RESPONSE OR TIMEOUT
    void set_trade_callback(std::function<void(std::string)> callback, std::chrono::seconds timeout = std::chrono::seconds(10)) {
      if (!trade_lock.try_lock_for(timeout)) {
        log->output("trade callback not fired in time. Allowing new callback setter access.");
        trade_callback("");
        trade_lock.lock();
      }
      trade_callback_original = callback;
      trade_callback = [&](std::string a) { trade_callback_original(a); trade_callback = nullptr; trade_lock.unlock(); };
    }
    void set_orderinfo_callback(std::function<void(OrderInfo)> callback, std::chrono::seconds timeout = std::chrono::seconds(10)) {
      if (!orderinfo_lock.try_lock_for(timeout)) {
        log->output("orderinfo callback not fired in time. Allowing new callback setter access.");
        OrderInfo failed_order;
        orderinfo_callback(failed_order);
        orderinfo_lock.lock();
      }
      orderinfo_callback_original = callback;
      orderinfo_callback = [&](OrderInfo a) { orderinfo_callback_original(a); orderinfo_callback = nullptr; orderinfo_lock.unlock(); };
    }
    void set_userinfo_callback(std::function<void(UserInfo)> callback, std::chrono::seconds timeout = std::chrono::seconds(10)) {
      if (!userinfo_lock.try_lock_for(timeout)) {
        log->output("userinfo callback not fired in time. Allowing new callback setter access.");
        UserInfo a;
        a.asset_net = 0;
        userinfo_callback(a);
        userinfo_lock.lock();
      }
      userinfo_callback_original = callback;
      userinfo_callback = [&](UserInfo a) { userinfo_callback_original(a); userinfo_callback = nullptr; userinfo_lock.unlock(); };
    }
    void set_filled_callback(std::function<void(double)> callback, std::chrono::seconds timeout = std::chrono::seconds(10)) {
      if (!filled_lock.try_lock_for(timeout)) {
        log->output("filled callback not fired in time. Allowing new callback setter access.");
        filled_callback(0);
        filled_lock.lock();
      }
      filled_callback_original = callback;
      filled_callback = [&](double a) { filled_callback_original(a); filled_callback = nullptr; filled_lock.unlock(); };
    }
    void set_pong_callback(std::function<void()> callback, std::chrono::seconds timeout = std::chrono::seconds(10)) {
      if (!pong_lock.try_lock_for(timeout)) {
        log->output("pong callback not fired in time. Allowing new callback setter access.");
        pong_callback();
        pong_lock.lock();
      }
      pong_callback_original = callback;
      pong_callback = [&]() { pong_callback_original(); pong_callback = nullptr; pong_lock.unlock(); };
    }

    std::string name;

    // timestamp representing time of last received message
    std::chrono::nanoseconds ts_since_last;

    // when true, letting handler know to make a new Exchange object
    bool reconnect;
  protected:
    std::shared_ptr<Config> config;
    std::shared_ptr<Log> log;

    // STREAMING CALLBACKS
    std::function<void(long, double, double, double)> ticker_callback;
    std::function<void(std::chrono::minutes, long, double, double, double, double, double, bool)> OHLC_callback;
    std::function<void()> open_callback;
    
    // ONE OFF CALLBACKS
    std::function<void(std::string)> trade_callback;
    std::function<void(std::string)> trade_callback_original;
    std::timed_mutex trade_lock;
    std::function<void(OrderInfo)> orderinfo_callback;
    std::function<void(OrderInfo)> orderinfo_callback_original;
    std::timed_mutex orderinfo_lock;
    std::function<void(UserInfo)> userinfo_callback;
    std::function<void(UserInfo)> userinfo_callback_original;
    std::timed_mutex userinfo_lock;
    std::function<void(double)> filled_callback;
    std::function<void(double)> filled_callback_original;
    std::timed_mutex filled_lock;
    std::function<void()> pong_callback;
    std::function<void()> pong_callback_original;
    std::timed_mutex pong_lock;
};
