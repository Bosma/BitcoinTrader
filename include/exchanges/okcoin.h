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

#include "json.hpp"

#include "websocket/websocket.h"
#include "utilities/curl.h"
#include "utilities/log.h"
#include "utilities/exchange_utils.h"
#include "exchanges/exchange.h"

class OKCoin : public Exchange {
public:
  const std::string OKCOIN_URL = "wss://real.okcoin.com:10440/websocket/okcoinapi";
  enum class Market { Future, Spot };
  static std::string market_s(const Market mkt) {
    return mkt == Market::Future ? "future" : "spot";
  }
  class Channel {
  public:
    enum class Status {
      Subscribing, Subscribed, Failed, Unsubscribed
    };
    Channel(std::string name, Status status) :
        name(name),
        status(status) { }
    std::string name;
    Status status;
    std::string last_message;
    std::chrono::nanoseconds last_message_time;

    std::string to_string() {
      return name + " (" + ts_to_string(last_message_time) + "): last message: " + last_message;
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

  std::map<std::string, Channel> channels;
  void subscribe_to_channel(std::string const &);
  void unsubscribe_to_channel(std::string const &);

  // HANDLERS FOR CHANNEL MESSAGES
  void OHLC_handler(const std::string&, const json&);
  void ticker_handler(const json&);
  virtual void userinfo_handler(const json&) = 0;
  virtual void orderinfo_handler(const json&) = 0;

  // GET MD5 HASH OF PARAMETERS
  std::string sign(const json&);
  std::string ampersand_list(const json&);

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
  std::string get_sig(const std::string&);
};
