#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

#include <curl/curl.h>
#include "../json/json.hpp"

bool check_until(std::function<bool()>, std::chrono::seconds = std::chrono::seconds(0), std::chrono::milliseconds = std::chrono::milliseconds(50));

enum Currency { BTC, USD };
enum Position { Long, Short };

std::chrono::nanoseconds timestamp_now();

class Ticker {
  public:
    Ticker(double b, double a, double l) :
      bid(b), ask(a), last(l) { }
    Ticker() { }
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

template <class T> class Atomic {
  public:
    void set(T new_x) {
      std::lock_guard<std::mutex> l(lock);
      x = new_x;
    }
    T get() {
      std::lock_guard<std::mutex> l(lock);
      return x;
    }
    void clear() {
      std::lock_guard<std::mutex> l(lock);
      T cleared;
      x = cleared;
    }
  private:
    T x;
    std::mutex lock;
};

double truncate_to(double, int);

std::string dtos(double, int);

size_t Curl_write_callback(void *contents, size_t size, size_t nmemb, std::string *s);

std::string curl_post(std::string, std::string = "");
