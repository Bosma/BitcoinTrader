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

class OKCoinSpot : public OKCoin {
public:
  struct UserInfo {
    double asset_net = 0;
    std::map<Currency, double> free;
    std::map<Currency, double> borrow;

    std::string to_string() {
      std::ostringstream os;
      os << "Equity: " << asset_net << ", BTC: " << free[Currency::BTC] << ", BTC (borrowed): " << borrow[Currency::BTC] << ", USD: " << free[Currency::USD] << ", USD (borrowed): " << borrow[Currency::USD];
      return os.str();
    }
  };
  struct BorrowInfo {
    std::string id = "";
    double amount = 0;
    double rate = 0;
    bool valid = false;
  };
  struct OrderInfo {
    double amount;
    double avg_price;
    std::string create_date;
    double filled_amount;
    std::string order_id;
    double price;
    OrderStatus status;
    std::string symbol;
    std::string type;
  };

  OKCoinSpot(std::string, std::shared_ptr<Log> log, std::shared_ptr<Config> config);

  void subscribe_to_ticker() override;
  void subscribe_to_depth() override;
  void subscribe_to_OHLC(std::chrono::minutes) override;
  bool subscribed_to_OHLC(std::chrono::minutes) override;
  bool backfill_OHLC(std::chrono::minutes, unsigned long) override;

  void market_buy(double, timestamp_t);
  void market_sell(double, timestamp_t);
  void limit_buy(double, double, timestamp_t);
  void limit_sell(double, double, timestamp_t);

  void set_userinfo_callback(std::function<void(const UserInfo&)> callback) {
    userinfo_callback = callback;
  }
  void set_orderinfo_callback(std::function<void(const OrderInfo&)> callback) {
    orderinfo_callback = callback;
  }
private:
  std::function<void(const UserInfo&)> userinfo_callback;
  std::function<void(const OrderInfo&)> orderinfo_callback;

  // HANDLERS FOR CHANNEL MESSAGES
  void orderinfo_handler(const json&) override;
  void userinfo_handler(const json&) override;

  void order(const std::string&, const std::string&, timestamp_t, const std::string& price = "");

  // INTERNAL REST BORROWING FUNCTIONS
  BorrowInfo borrow(Currency, double);
  double close_borrow(Currency);
  json lend_depth(Currency);
  json borrows_info(Currency);
  json unrepayments_info(Currency);
  json borrow_money(Currency, double, double);
  json repayment(const std::string&);
};
