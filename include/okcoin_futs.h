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
    enum ContractType { Weekly, NextWeekly, Quarterly };
    enum OrderType { OpenLong = 1, OpenShort = 2,
      CloseLong = 3, CloseShort = 4 };
    struct UserInfo {
      double equity = 0;
      double margin = 0;
      double realized = 0;
      double unrealized = 0;
    };
    struct OrderInfo {
      double amount;
      std::string contract_name;
      std::string create_date;
      double filled_amount;
      double fee;
      int lever_rate;
      std::string order_id;
      double price;
      double avg_price;
      std::string status;
      std::string symbol;
      OrderType type;
      int unit_amount;
    };

    OKCoinFuts(ContractType, std::shared_ptr<Log> log, std::shared_ptr<Config> config);
    
    void subscribe_to_ticker();
    void subscribe_to_OHLC(std::chrono::minutes);
    void open(Position, double, double, double);
    void close(Position, double, double, double);
    void cancel_order(std::string);
    void orderinfo(std::string);

    void set_userinfo_callback(std::function<void(UserInfo)> callback) {
      userinfo_callback = callback;
    }
    void set_orderinfo_callback(std::function<void(OrderInfo)> callback) {
      orderinfo_callback = callback;
    }

  private:
    ContractType contract_type;

    std::function<void(UserInfo)> userinfo_callback;
    std::function<void(OrderInfo)> orderinfo_callback;

    void order(OrderType, double, double, double, bool);

    void orderinfo_handler(nlohmann::json);
    void userinfo_handler(nlohmann::json);

    // convert contract type into string for ws messages
    static const std::string contract_s(ContractType type) {
      switch (type) {
        case Weekly : return "this_week"; break;
        case NextWeekly : return "next_week"; break;
        case Quarterly : return "quarter"; break;
      }
    }
};
