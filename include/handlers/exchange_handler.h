#pragma once

#include <string>
#include <memory>
#include <atomic>

#include "utilities/log.h"
#include "utilities/config.h"
#include "exchanges/exchange.h"
#include "utilities/config.h"
#include "trading/mktdata.h"

class ExchangeHandler {
public:
  ExchangeHandler(std::string name, std::shared_ptr<Config> config, std::string exchange_log_key, std::string trading_log_key, std::string execution_log_key) :
      name(name),
      exchange_log(std::make_shared<Log>((*config)[exchange_log_key])),
      trading_log(std::make_shared<Log>((*config)[trading_log_key])),
      execution_log(std::make_shared<Log>((*config)[execution_log_key])),
      config(config),
      cancel_checking(false) {
  }
  std::string name;

  std::shared_ptr<Log> exchange_log;
  std::shared_ptr<Log> trading_log;
  std::shared_ptr<Log> execution_log;

  std::shared_ptr<Config> config;
  std::shared_ptr<Exchange> exchange;

  std::map<std::chrono::minutes, MktData> mktdata;
  std::vector<std::shared_ptr<SignalStrategy>> signal_strategies;
  // get vector of above strategies together
  std::vector<std::shared_ptr<Strategy>> strategies() {
    std::vector<std::shared_ptr<Strategy>> s;
    s.insert(s.end(),
             signal_strategies.begin(), signal_strategies.end());
    return s;
  }
  Atomic<Ticker> tick;
  Atomic<Depth> depth;

  std::mutex reconnect;
  std::atomic<bool> cancel_checking;

  virtual void set_up_and_start() = 0;
  virtual void manage_positions(double) = 0;
};