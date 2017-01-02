#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

#include <curl/curl.h>
#include <json.hpp>


enum Currency { BTC, CNY };

struct UserInfo { double asset_net = 0; double free_btc = 0; double free_cny = 0; double borrow_btc = 0; double borrow_cny = 0; };

struct BorrowInfo { std::string id = ""; double amount = 0; double rate = 0; };

class Channel {
  public:
    Channel(std::string name, std::string status) :
      name(name),
      status(status) { }

    void subscribe();

    std::string name;
    std::string status;
    std::string last_message;
};

class Ticker {
  public:
    Ticker(long ts, double b, double a, double l) :
      timestamp(ts), bid(b), ask(a), last(l) { }
    Ticker() { }
    long timestamp;
    double bid;
    double ask;
    double last;
};

class OHLC {
  public:
    OHLC(long timestamp, double open, double high,
        double low, double close, double volume) :
      timestamp(timestamp),
      open(open),
      high(high),
      low(low),
      close(close),
      volume(volume) { }

    long timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
             // strategy name
    std::map<std::string,
                      // indicator name
             std::map<std::string,
                               // column name
                      std::map<std::string,
                               // indicator value 
                               double>>> indis;

    std::string to_string() {
      std::ostringstream os;
      os << "timestamp=" << timestamp <<
        ",open=" << open << ",high=" << high <<
        ",low=" << low << ",close=" << close <<
        ",volume=" << volume;
      for (auto strategy : indis) {
        os << "," << strategy.first << "={ ";
        for (auto indicator : strategy.second) {
          os << indicator.first << "=(";
          for (auto column : indicator.second)
            os << column.first << "=" << column.second;
          os << ")";
        }
        os << " }";
      }
      return os.str();
    }
};

class OrderInfo {
  public:
    OrderInfo(double amount,
              double avg_price,
              std::string create_date,
              double filled_amount,
              std::string order_id,
              double price,
              std::string status,
              std::string symbol,
              std::string type) :
      amount(amount), avg_price(avg_price), create_date(create_date),
      filled_amount(filled_amount), order_id(order_id), price(price),
      status(status), symbol(symbol), type(type) { }
    OrderInfo() :
      amount(0), avg_price(0), create_date(""),
      filled_amount(0), order_id(""), price(0),
      status(""), symbol(""), type("") { }

    double amount;
    double avg_price;
    std::string create_date;
    double filled_amount;
    std::string order_id;
    double price;
    std::string status;
    std::string symbol;
    std::string type;

    std::string to_string() {
      std::ostringstream os;
      os << "amount: " << amount << ", avg_price: " << avg_price
        << ", create_date: " << create_date << ", filled_amount: "
        << filled_amount << ", order_id: " << order_id
        << ", price: " << ", status: " << status << ", symbol: "
        << symbol << ", type: " << type;
      return os.str();
    }
};

long optionally_to_long(nlohmann::json);
double optionally_to_double(nlohmann::json);
int optionally_to_int(nlohmann::json);
template <typename T> std::string opt_to_string(nlohmann::json object) {
  if (object.is_string()) {
    return object.get<std::string>();
  }
  else
    return std::to_string(object.get<T>());
}

double truncate_to(double, int);

std::string dtos(double, int);

std::chrono::nanoseconds timestamp_now();

size_t Curl_write_callback(void *contents, size_t size, size_t nmemb, std::string *s);

std::string curl_post(std::string, std::string = "");
