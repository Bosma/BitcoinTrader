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
    virtual void userinfo() = 0;
    virtual void ping() = 0;
    virtual void backfill_OHLC(std::chrono::minutes, int) = 0;
    std::string status() {
      std::ostringstream os;
      os << name << ": " << std::endl;
      return os.str();
    }

    void set_ticker_callback(std::function<void(Ticker)> callback) {
      ticker_callback = callback;
    }
    void set_OHLC_callback(std::function<void(std::chrono::minutes, OHLC)> callback) {
      OHLC_callback = callback;
    }
    void set_open_callback(std::function<void()> callback) {
      open_callback = callback;
    }
    void set_trade_callback(std::function<void(std::string)> callback) {
      trade_callback = callback;
    }
    void set_filled_callback(std::function<void(double)> callback) {
      filled_callback = callback;
    }
    void set_pong_callback(std::function<void()> callback) {
      pong_callback = callback;
    }

    std::string name;

    // timestamp representing time of last received message
    std::chrono::nanoseconds ts_since_last;

    // when true, letting handler know to make a new Exchange object
    bool reconnect;
  protected:
    std::shared_ptr<Config> config;
    std::shared_ptr<Log> log;

    std::function<void(Ticker)> ticker_callback;
    std::function<void(std::chrono::minutes, OHLC)> OHLC_callback;
    std::function<void()> open_callback;
    std::function<void(std::string)> trade_callback;
    std::function<void(double)> filled_callback;
    std::function<void()> pong_callback;
};
