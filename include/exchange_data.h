#pragma once

#include <string>
#include <memory>

#include "../include/exchange.h"
#include "../include/exchange_utils.h"
#include "../include/mktdata.h"

struct ExchangeMeta {
  std::string name;
  std::shared_ptr<Exchange> exchange;
  std::shared_ptr<Log> log;
  std::map<std::chrono::minutes, std::shared_ptr<MktData>> mktdata;
  Atomic<Ticker> tick;
  std::function<void()> set_up_and_start;
  std::function<std::string()> print_userinfo;
};
template <class T> class ExchangeData {
  public:
    ExchangeData(std::string name, std::shared_ptr<Log> log) :
      meta(std::make_shared<ExchangeMeta>())
    {
      meta->name = name;
      meta->log = log;
    };
    Atomic<typename T::UserInfo> user_info;
    std::shared_ptr<T> exchange;
    std::shared_ptr<ExchangeMeta> meta;

    void reset() {
      user_info.clear();
      meta->tick.clear();
      meta->exchange = nullptr;
    }
};

