#pragma once

#include <string>
#include <sstream>
#include <map>
#include <chrono>
#include <memory>
#include <fstream>

#include <openssl/md5.h>

#include "json.hpp"

#include "websocket/websocket.h"
#include "utilities/log.h"
#include "utilities/exchange_utils.h"
#include "exchanges/okcoin.h"

class OKCoinFuts : public OKCoin {
public:
  enum class ContractType { Weekly, NextWeekly, Quarterly };
  enum class OrderType { OpenLong = 1, OpenShort = 2,
    CloseLong = 3, CloseShort = 4 };
  struct UserInfo {
    double equity = 0;
    double margin = 0;
    double realized = 0;
    double unrealized = 0;

    std::string to_string() {
      std::ostringstream os;
      os << "Equity: " << equity << ", Margin: " << margin << ", Realized: " << realized << ", Unrealized: " << unrealized;
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
    int lever_rate = 10;
    double margin_call_price = 0;
    bool valid = false;
  };

  OKCoinFuts(std::string, ContractType, std::shared_ptr<Log> log, std::shared_ptr<Config> config);

  void subscribe_to_ticker();
  void subscribe_to_OHLC(std::chrono::minutes);
  bool subscribed_to_OHLC(std::chrono::minutes);
  void open(Position, double, double, int, std::chrono::nanoseconds);
  void close(Position, double, double, int, std::chrono::nanoseconds);
  void order(OrderType, double, double, int, bool, std::chrono::nanoseconds);
  void cancel_order(const std::string&, std::chrono::nanoseconds);
  void orderinfo(const std::string&, std::chrono::nanoseconds);

  void set_userinfo_callback(std::function<void(const UserInfo&)> callback) {
    userinfo_callback = callback;
  }
  void set_orderinfo_callback(std::function<void(const OrderInfo&)> callback) {
    orderinfo_callback = callback;
  }

  // REST commands
  FuturePosition positions();
  bool backfill_OHLC(std::chrono::minutes, unsigned long);

private:
  ContractType contract_type;

  std::function<void(const UserInfo&)> userinfo_callback;
  std::function<void(const OrderInfo&)> orderinfo_callback;

  void orderinfo_handler(const json&);
  void userinfo_handler(const json&);

  // convert contract type into string for ws messages
  static const std::string contract_s(const ContractType type) {
    switch (type) {
      case ContractType::Weekly : return "this_week";
      case ContractType::NextWeekly : return "next_week";
      case ContractType::Quarterly : return "quarter";
    }
  }
};
