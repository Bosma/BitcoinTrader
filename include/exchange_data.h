#pragma once

#include <string>
#include <memory>

#include "../include/exchange.h"
#include "../include/exchange_utils.h"
#include "../include/mktdata.h"

class ExchangeMeta {
  public:
    ExchangeMeta(std::string name, std::shared_ptr<Log> log) :
        name(name), log(log) {
    }
    std::string name;
    std::shared_ptr<Log> log;

    std::shared_ptr<Exchange> exchange;

    std::map<std::chrono::minutes, std::shared_ptr<MktData>> mktdata;
    Atomic<Ticker> tick;

    std::function<void()> set_up_and_start;
    std::function<std::string()> print_userinfo;

    std::mutex reconnect;
};
// TODO: try out ExchangeData as a subclass of ExchangeMeta
template <class T> class ExchangeData {
  public:
    ExchangeData(std::string name, std::shared_ptr<Log> log) :
        meta(std::make_shared<ExchangeMeta>(name, log)) {
    }

    Atomic<typename T::UserInfo> user_info;

    std::shared_ptr<T> exchange;

    // TODO: does this have to be a shared_ptr?
    std::shared_ptr<ExchangeMeta> meta;

    template <typename ...Params>
    void reset(Params&&... params) {
      user_info.clear();
      meta->tick.clear();

      exchange = std::make_shared<T>(std::forward<Params>(params)...);
      meta->exchange = exchange;
    }
  // TODO: fix indenting of every file
};

