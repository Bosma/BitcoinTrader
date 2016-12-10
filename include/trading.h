#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <map>
#include <cmath>
#include <boost/circular_buffer.hpp>
#include <iostream>
#include <iomanip>

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
    std::map<std::string, std::map<std::string, double>> indis;

    std::string to_string() {
      std::ostringstream os;
      os << "timestamp=" << timestamp <<
        ",open=" << open << ",high=" << high <<
        ",low=" << low << ",close=" << close <<
        ",volume=" << volume;
      for (auto i : indis) {
        os << "," << i.first << "=(";
        for (auto pt : i.second)
          os << " " << pt.first << "=" << pt.second;
        os << " )";
      }
      return os.str();
    }
};

class Indicator {
  public:
    Indicator(std::string name) : name(name) { }
    virtual std::map<std::string, double> calculate(std::shared_ptr<boost::circular_buffer<std::shared_ptr<OHLC>>>) = 0;
    std::string name;
};

class MovingAverage : public Indicator {
  public:
    MovingAverage(std::string name, int period) :
      Indicator(name), period(period) { }

    std::map<std::string, double> calculate(std::shared_ptr<boost::circular_buffer<std::shared_ptr<OHLC>>> bars) {
      std::map<std::string, double> values;
      double ma_value;
      if (period <= bars->size()) {
        std::vector<std::shared_ptr<OHLC>> period_bars(bars->end() - period, bars->end());

        double sum = 0;
        for (auto bar : period_bars)
          sum += bar->close;

        ma_value = sum / period;
      }
      else
        ma_value = 0;

      values["mavg"] = ma_value;
      return values;
    }
    int period;
};

class BollingerBands : public Indicator {
  public:
    BollingerBands(std::string name, int period = 20, double sd = 2) :
      Indicator(name), period(period), sd(sd) { }

    std::map<std::string, double> calculate(std::shared_ptr<boost::circular_buffer<std::shared_ptr<OHLC>>> bars) {
      std::map<std::string, double> values;
      if (period <= bars->size()) {
        std::vector<std::shared_ptr<OHLC>> period_bars(bars->end() - period, bars->end());
        auto welford = welfords_algorithm(period_bars);

        double mavg = welford.first;
        double std = sqrt(welford.second);

        values["mavg"] = mavg;
        values["up"] = mavg + (std * sd);
        values["dn"] = mavg - (std * sd);
        values["bw"] = values["up"] - values["dn"];
      }
      else {
        values["mavg"] = 0;
        values["up"] = 0;
        values["dn"] = 0;
        values["bw"] = 0;
      }
      return values;
    }

    std::pair<double, double> welfords_algorithm(std::vector<std::shared_ptr<OHLC>> bars) {
      int n = 0;
      double mean = 0;
      double M2 = 0;

      for (auto x : bars) {
        n += 1;
        double delta = x->close - mean;
        mean += delta / n;
        M2 += delta * (x->close - mean);
      }

      return std::make_pair(mean, M2 / (n - 1));
    }

    int period;
    double sd;
};

class MktData {
  public:
    MktData(double bar_period) :
      bars(new boost::circular_buffer<std::shared_ptr<OHLC>>(50)),
      indicators(),
      bar_period(bar_period) { }

    void add(std::shared_ptr<OHLC> bar) {
      // don't add bars of the same timestamp
      bool found = false;
      for (auto x : *bars)
        if (x->timestamp == bar->timestamp)
          found = true;

      if (bars->empty() || !found) {
        // if we receive a bar that's not contiguous
        // our old data is useless
        if (!bars->empty() &&
            bar->timestamp != bars->back()->timestamp + (bar_period * 60000))
          bars->clear();

        bars->push_back(bar);

        // for each indicator
        // calculate the indicator value
        // from the bars (with the new value)
        for (auto &indi : indicators)
          bar->indis[indi->name] = indi->calculate(bars);

        if (new_bar_callback)
          new_bar_callback(bar);
      }
    }

    void set_new_bar_callback(std::function<void(std::shared_ptr<OHLC>)> callback) {
      new_bar_callback = callback;
    }

    void add_indicators(std::vector<std::shared_ptr<Indicator>> to_add) {
      indicators.insert(indicators.end(), to_add.begin(), to_add.end());
    }

    std::shared_ptr<boost::circular_buffer<std::shared_ptr<OHLC>>> bars;
  private:
    std::vector<std::shared_ptr<Indicator>> indicators;
    std::function<void(std::shared_ptr<OHLC> bar)> new_bar_callback;
    
    // bar period in minutes
    double bar_period;
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

class Stop {
  public:
    Stop(double price,
        std::string name, std::string direction) :
      price(price), name(name), direction(direction), triggered(false) { }

    std::string to_string() {
      std::ostringstream os;
      os << std::fixed << std::setprecision(4) << name << ": " << price;
      return os.str();
    }

    virtual bool trigger(Ticker tick) = 0;
    virtual std::string action() = 0;

    double price;
    std::string name;
    std::string direction;
    bool triggered;
  protected:
    bool stop_trigger(Ticker tick) {
      if (direction == "long") {
        if (tick.bid <= price) {
          triggered = true;
        }
      }
      else {
        if (tick.bid >= price) {
          triggered = true;
        }
      }
      return triggered;
    }
};

class StopLoss : public Stop {
  public:
    StopLoss(double price, std::string direction) :
      Stop(price, "stop-loss", direction) { }

    bool trigger(Ticker tick) {
      return stop_trigger(tick);
    }

    std::string action() { return "STOPPED OUT"; }
};

class TrailingStop : public Stop {
  public:
    TrailingStop(double price, std::string direction, double stop_distance) :
      Stop(price, "trailing-stop", direction), stop_distance(stop_distance) { }
  private:
    double stop_distance;

    bool trigger(Ticker tick) {
      // trail the stop if necessary
      if (tick.bid - price > stop_distance) {
        price = tick.bid - stop_distance;
      }
      return stop_trigger(tick);
    }

    std::string action() { return "STOPPED OUT"; }
};

class TakeProfit : public Stop {
  public:
    TakeProfit(double price, std::string direction) :
      Stop(price, "take-profit", direction) { }

    bool trigger(Ticker tick) {
      if (direction == "long") {
        if (tick.bid >= price) {
          triggered = true;
        }
      }
      else {
        if (tick.ask <= price) {
          triggered = true;
        }
      }
      return triggered;
    }

    std::string action() { return "TAKING PROFIT"; }
};

class SimulatedLimit {
  public:
    SimulatedLimit(double amount, double price, std::string direction, double timestamp) :
      amount(amount), price(price), direction(direction), triggered(false), entered_timestamp(timestamp) { }

    bool trigger(Ticker tick) {
      if (direction == "long") {
        if (tick.last < price ||
            tick.ask <= price) {
          triggered = true;
        }
      }
      else {
        if (tick.last > price ||
            tick.bid >= price) {
          triggered = true;
        }
      }
      return triggered;
    }
    double amount;
    double price;
    std::string direction;
    bool triggered;
    long entered_timestamp;
};
