#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

#include <boost/lexical_cast.hpp>

#include "json.hpp"

using json = nlohmann::json;
using timestamp_t = std::chrono::high_resolution_clock::time_point;

bool check_until(std::function<bool()>, timestamp_t = {}, std::chrono::milliseconds = std::chrono::milliseconds(50));

enum class Currency { BTC, USD };
enum class Position { Long, Short };

timestamp_t timestamp_now();

class Ticker {
public:
  Ticker(double b, double a, double l, timestamp_t ts) :
      bid(b), ask(a), last(l), timestamp(ts) { }
  Ticker() { }
  double bid;
  double ask;
  double last;
  timestamp_t timestamp;
};

class Depth {
public:
  struct Order {
    Order(double price, int volume) : price(price), volume(volume) { };

    bool operator<(const Order& rhs) {
      return price < rhs.price;
    }

    std::string to_string() const {
      return std::to_string(price) + ":" + std::to_string(volume);
    }

    double price;
    int volume;
  };
  using Orders = std::vector<Order>;

  Depth(Orders bids, Orders asks, timestamp_t ts) :
      bids(bids), asks(asks), timestamp(ts) { }
  Depth() { }

  std::string to_string() const {
    std::string s = std::to_string(timestamp.time_since_epoch().count());
    s += "|" + std::accumulate(std::next(bids.begin()),
                               bids.end(),
                               bids[0].to_string(),
                               [](const std::string& a, const Order& b) {
                                 return a + ";" + b.to_string();
                               });
    s += "|" + std::accumulate(std::next(asks.begin()),
                               asks.end(),
                               asks[0].to_string(),
                               [](const std::string& a, const Order& b) {
                                 return a + ";" + b.to_string();
                               });
    return s;
  }

  Orders bids;
  Orders asks;
  timestamp_t timestamp;
};

std::string ts_to_string(timestamp_t);

class IndicatorValue {
public:
  IndicatorValue() : is_set(false) {}

  void set(const double new_value) {
    is_set = true;
    value = new_value;
  }
  double get() const {
    return value;
  }
  bool has_been_set() const {
    return is_set;
  }

  std::string to_string() const {
    if (is_set)
      return std::to_string(value);
    else
      return "NA";
  }

  friend std::ostream& operator<<(std::ostream& stream, const IndicatorValue iv) {
    if (iv.has_been_set())
      stream << iv.value;
    else
      stream << "NA";
    return stream;
  }

private:
  double value;
  bool is_set;
};



class OHLC {
public:
  OHLC(timestamp_t timestamp, double open, double high,
       double low, double close, double volume) :
      timestamp(timestamp),
      open(open),
      high(high),
      low(low),
      close(close),
      volume(volume) { }

  timestamp_t timestamp;
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
              IndicatorValue>>> indis;

  // returns if all indicators are set for a given strategy
  bool all_indis_set(std::string name) const {
    bool all_set = true;

    if (indis.count(name)) {
      for (auto& indicator : indis.at(name)) {
        for (auto& column : indicator.second)
          all_set = all_set && column.second.has_been_set();
      }
    }
    return all_set;
  }

  std::vector<std::string> to_columns() {
    std::vector<std::string> vs = {
        "timestamp", "open", "high", "low", "close", "volume"
    };
    for (auto& strategy : indis)
      for (auto& indicator : strategy.second)
        for (auto& column : indicator.second)
          vs.push_back(strategy.first + "." + indicator.first + "." + column.first);
    return vs;
  }

  std::vector<std::string> to_csv() {
    std::vector<std::string> vs = {
        std::to_string(timestamp.time_since_epoch().count()),
        std::to_string(open),
        std::to_string(high),
        std::to_string(low),
        std::to_string(close),
        std::to_string(volume)
    };
    for (auto& strategy : indis)
      for (auto& indicator : strategy.second)
        for (auto& column : indicator.second)
          vs.push_back(column.second.to_string());
    return vs;
  }
};

template <typename T> std::string opt_to_string(const json& object) {
  if (object.is_string()) {
    return object.get<std::string>();
  }
  else
    return std::to_string(object.get<T>());
}
template <typename T>
T optionally_to(const json& j) {
  if (j.is_string())
    return boost::lexical_cast<T>(j.get<std::string>());
  else
    return j;
}

template <class T> class Atomic {
public:
  Atomic() : is_set(false) { };
  Atomic(const Atomic&) = delete;
  Atomic& operator=(const Atomic&) = delete;

  void set(T new_x) {
    std::lock_guard<std::mutex> l(lock);
    is_set = true;
    x = new_x;
  }
  T get() {
    std::lock_guard<std::mutex> l(lock);
    return x;
  }
  void clear() {
    std::lock_guard<std::mutex> l(lock);
    T cleared{};
    is_set = false;
    x = cleared;
  }
  bool has_been_set() {
    return is_set;
  }
private:
  T x;
  std::mutex lock;
  bool is_set;
};

double truncate_to(double, int);

std::string dtos(double, int);

size_t Curl_write_callback(void *contents, size_t size, size_t nmemb, std::string *s);