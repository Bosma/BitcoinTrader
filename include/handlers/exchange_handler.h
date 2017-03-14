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
  ExchangeHandler(std::string name, std::shared_ptr<Log> log, std::shared_ptr<Log> trading_log, std::shared_ptr<Config> config) :
      name(name), log(log), trading_log(trading_log), config(config), cancel_checking(false) {
  }
  std::string name;
  std::shared_ptr<Log> log;
  std::shared_ptr<Log> trading_log;
  std::shared_ptr<Config> config;

  std::shared_ptr<Exchange> exchange;

  std::map<std::chrono::minutes, MktData> mktdata;
  std::vector<std::shared_ptr<Strategy>> strategies;
  Atomic<Ticker> tick;

  std::mutex reconnect;
  std::atomic<bool> cancel_checking;

  virtual void set_up_and_start() = 0;
  virtual void manage_positions(double) = 0;
};