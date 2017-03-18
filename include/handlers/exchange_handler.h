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
  ExchangeHandler(std::string name, std::shared_ptr<Config> config) :
      name(name), config(config), cancel_checking(false) {
  }
  std::string name;
  std::map<std::string, std::shared_ptr<Log>> logs;
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

  void make_log(std::string name, std::string config_key) {
    logs[name] = std::make_shared<Log>((*config)[config_key]);
  }

  std::mutex reconnect;
  std::atomic<bool> cancel_checking;

  virtual void set_up_and_start() = 0;
  virtual void manage_positions(double) = 0;
};