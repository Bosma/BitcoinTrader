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

class OKCoinSpot : public OKCoin {
  public:
    struct UserInfo {
      double asset_net = 0;
      std::map<Currency, double> free;
      std::map<Currency, double> borrow;
    };
    struct BorrowInfo {
      std::string id = "";
      double amount = 0;
      double rate = 0;
    };
    struct OrderInfo {
      double amount;
      double avg_price;
      std::string create_date;
      double filled_amount;
      std::string order_id;
      double price;
      std::string status;
      std::string symbol;
      std::string type;
    };

    OKCoinSpot(std::shared_ptr<Log> log, std::shared_ptr<Config> config);
    
    void subscribe_to_ticker();
    void subscribe_to_OHLC(std::chrono::minutes);
    void market_buy(double);
    void market_sell(double);
    void limit_buy(double, double);
    void limit_sell(double, double);

    void set_userinfo_callback(std::function<void(UserInfo)> callback) {
      userinfo_callback = callback;
    }
    void set_orderinfo_callback(std::function<void(OrderInfo)> callback) {
      orderinfo_callback = callback;
    }

  private:
    // WEBSOCKET CALLBACKS
    void on_message(std::string const &);

    std::function<void(UserInfo)> userinfo_callback;
    std::function<void(OrderInfo)> orderinfo_callback;

    void order(std::string, std::string, std::string price = "");

    // INTERNAL REST BORROWING FUNCTIONS
    BorrowInfo borrow(Currency, double);
    double close_borrow(Currency);
    json lend_depth(Currency);
    json borrows_info(Currency);
    json unrepayments_info(Currency);
    json borrow_money(Currency, double, double);
    json repayment(std::string);

    // HANDLERS FOR CHANNEL MESSAGES
    void OHLC_handler(std::string, nlohmann::json);
    void ticker_handler(nlohmann::json);
    void orderinfo_handler(nlohmann::json);
};
