#pragma once

#include <string>
#include <sstream>
#include <map>
#include <chrono>
#include <memory>
#include <fstream>
#include <regex>
#include <numeric>

#include <openssl/md5.h>

#include "../json/json.hpp"

#include "../include/websocket.h"
#include "../include/curl.h"
#include "../include/log.h"
#include "../include/exchange_utils.h"
#include "../include/exchange.h"

class OKCoin : public Exchange {
public:
  const std::string OKCOIN_URL = "wss://real.okcoin.com:10440/websocket/okcoinapi";
  enum Market { Future, Spot };
  static std::string market_s(Market mkt) {
    return mkt == Future ? "future" : "spot";
  }
  struct Channel {
    Channel(std::string name, std::string status) :
        name(name),
        status(status) { }
    std::string name;
    std::string status;
    std::string last_message;
    std::chrono::nanoseconds last_message_time;

    std::string to_string() {
      return name + " (" + ts_to_string(last_message_time) + "): " + status + ", last message: " + last_message;
    }
  };
  enum class OrderStatus {
    Cancelled = -1,
    Unfilled = 0,
    PartiallyFilled = 1,
    FullyFilled = 2,
    CancelRequest = 4,
    Failed = 5
  };
  static std::string status_s(OrderStatus o) {
    switch (o) {
      case OrderStatus::Cancelled : return "cancelled";
      case OrderStatus::Unfilled : return "unfilled";
      case OrderStatus::PartiallyFilled : return "partially filled";
      case OrderStatus::FullyFilled : return "fully filled";
      case OrderStatus::CancelRequest : return "cancel requested";
      case OrderStatus::Failed : return "failed";
    }
  }

  OKCoin(std::string, Market, std::shared_ptr<Log>, std::shared_ptr<Config>);

  void start();

  void ping();

  void userinfo(std::chrono::nanoseconds);

  // backfill OHLC period
  virtual bool backfill_OHLC(std::chrono::minutes, unsigned long) = 0;

  std::string status();

  bool connected() {
    return ws.get_status() == websocket::Status::Open;
  }

protected:
  std::string api_key;
  std::string secret_key;
  websocket ws;
  Market market;

  void on_open();
  void on_message(std::string const &);
  void on_close();
  void on_fail();
  void on_error(std::string const &);

  std::map<std::string, std::string> error_reasons;
  void populate_error_reasons();

  std::map<std::string, std::shared_ptr<Channel>> channels;
  void subscribe_to_channel(std::string const &);
  void unsubscribe_to_channel(std::string const &);

  // HANDLERS FOR CHANNEL MESSAGES
  void OHLC_handler(std::string, nlohmann::json);
  void ticker_handler(nlohmann::json);
  virtual void userinfo_handler(nlohmann::json) = 0;
  virtual void orderinfo_handler(nlohmann::json) = 0;

  // GET MD5 HASH OF PARAMETERS
  std::string sign(json);
  std::string ampersand_list(json);

  static const std::string period_s(std::chrono::minutes period) {
    if (period == std::chrono::minutes(1))
      return "1min";
    if (period == std::chrono::minutes(15))
      return "15min";
    else
      return "";
  }
  static const std::chrono::minutes period_m(std::string kline) {
    if (kline == "1min")
      return std::chrono::minutes(1);
    if (kline == "15min")
      return std::chrono::minutes(15);
    else
      return std::chrono::minutes(0);
  }
private:
  std::string get_sig(std::string);
};
