#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

#include "json.hpp"

using json = nlohmann::json;

bool check_until(std::function<bool()>, std::chrono::nanoseconds = std::chrono::nanoseconds(0), std::chrono::milliseconds = std::chrono::milliseconds(50));

enum class Currency { BTC, USD };
enum class Position { Long, Short };

std::chrono::nanoseconds timestamp_now();

class Ticker {
public:
  Ticker(double b, double a, double l, std::chrono::nanoseconds ts) :
      bid(b), ask(a), last(l), timestamp(ts) { }
  Ticker() { }
  double bid;
  double ask;
  double last;
  std::chrono::nanoseconds timestamp;
};

class Depth {
public:
  struct Order {
    Order(double price, int volume) : price(price), volume(volume) { };

    bool operator<(const Order& rhs) {
      return price < rhs.price;
    }

    double price;
    int volume;
  };
  using Orders = std::vector<Order>;

  Depth(Orders bids, Orders asks, std::chrono::nanoseconds ts) :
      bids(bids), asks(asks), timestamp(ts) { }
  Depth() { }

  Orders bids;
  Orders asks;
  std::chrono::nanoseconds timestamp;
};

std::string ts_to_string(std::chrono::nanoseconds);

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
  bool operator<(const IndicatorValue& other) const {
    return value < other.value;
  }
  bool operator>(const IndicatorValue& other) const {
    return value > other.value;
  }
  bool operator<=(const IndicatorValue& other) const {
    return value <= other.value;
  }
  bool operator>=(const IndicatorValue& other) const {
    return value >= other.value;
  }
  bool operator==(const IndicatorValue& other) const {
    return value == other.value;
  }
  bool operator!=(const IndicatorValue& other) const {
    return value != other.value;
  }
  friend std::ostream& operator<<(std::ostream& stream, const IndicatorValue iv) {
    if (iv.has_been_set())
      stream << iv.value;
    else
      stream << "N/A";
    return stream;
  }

private:
  double value;
  bool is_set;
};



class OHLC {
public:
  OHLC(std::chrono::nanoseconds timestamp, double open, double high,
       double low, double close, double volume) :
      timestamp(timestamp),
      open(open),
      high(high),
      low(low),
      close(close),
      volume(volume) { }

  std::chrono::nanoseconds timestamp;
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

  std::string to_string() {
    std::ostringstream os;
    os << "timestamp=" << ts_to_string(timestamp) <<
       ",open=" << open << ",high=" << high <<
       ",low=" << low << ",close=" << close <<
       ",volume=" << volume;

    for (auto& strategy : indis)
      for (auto& indicator : strategy.second)
        for (auto& column : indicator.second)
          os << "," << strategy.first << "." << indicator.first << "." << column.first << "=" << column.second;

    return os.str();
  }
};

long optionally_to_long(const json&);
double optionally_to_double(const json&);
int optionally_to_int(const json&);
template <typename T> std::string opt_to_string(const json& object) {
  if (object.is_string()) {
    return object.get<std::string>();
  }
  else
    return std::to_string(object.get<T>());
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