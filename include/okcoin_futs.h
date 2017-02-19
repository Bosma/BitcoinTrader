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

      std::string to_string() {
        std::ostringstream os;
        os << "Equity: " << equity << ", Margin: " << margin << std::endl;
        os << "Realized: " << realized << ", Unrealized: " << unrealized << std::endl;
        return os.str();
      }
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
      OrderStatus status;
      std::string symbol;
      OrderType type;
      int unit_amount;
    };
    struct FuturePosition {
      struct Position {
        int contracts = 0;
        int contracts_can_close = 0;
        double avg_open_price = 0;
        double cost_price = 0;
        double realized_profit = 0;
      };
      Position buy;
      Position sell;
      std::string contract_id = "";
      std::string create_date = "";
      int lever_rate = 0;
      double margin_call_price = 0;
      bool valid = false;
    };

    OKCoinFuts(std::string, ContractType, std::shared_ptr<Log> log, std::shared_ptr<Config> config);
    
    void subscribe_to_ticker();
    void subscribe_to_OHLC(std::chrono::minutes);
    void open(Position, double, double, int, std::chrono::nanoseconds);
    void close(Position, double, double, int, std::chrono::nanoseconds);
    void order(OrderType, double, double, int, bool, std::chrono::nanoseconds);
    void cancel_order(std::string, std::chrono::nanoseconds);
    void orderinfo(std::string, std::chrono::nanoseconds);

    void set_userinfo_callback(std::function<void(UserInfo)> callback) {
      userinfo_callback = callback;
    }
    void set_orderinfo_callback(std::function<void(OrderInfo)> callback) {
      orderinfo_callback = callback;
    }

    // REST commands
    FuturePosition positions();
    void backfill_OHLC(std::chrono::minutes, int);

  private:
    ContractType contract_type;

    std::function<void(UserInfo)> userinfo_callback;
    std::function<void(OrderInfo)> orderinfo_callback;

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
