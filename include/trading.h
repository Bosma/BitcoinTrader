#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <map>
#include <cmath>
#include <iostream>
#include <iomanip>

#include <boost/circular_buffer.hpp>

#include "../include/exchange_utils.h"

class Indicator {
public:
  Indicator(std::string name, int period) : name(name), period(period) { }
  virtual std::map<std::string, double> calculate(std::shared_ptr<boost::circular_buffer<OHLC>>) = 0;
  std::string name;
  int period;
};

class MovingAverage : public Indicator {
public:
  MovingAverage(std::string name, int period) :
      Indicator(name, period) { }

  std::map<std::string, double> calculate(std::shared_ptr<boost::circular_buffer<OHLC>> bars) {
    std::map<std::string, double> values;
    double ma_value;
    if (period <= bars->size()) {
      std::vector<OHLC> period_bars(bars->end() - period, bars->end());

      double sum = 0;
      for (auto bar : period_bars)
        sum += bar.close;

      ma_value = sum / period;
    }
    else
      ma_value = 0;

    values["mavg"] = ma_value;
    return values;
  }
};

class BollingerBands : public Indicator {
public:
  BollingerBands(std::string name, int period = 20, double sd = 2) :
      Indicator(name, period), sd(sd) { }

  std::map<std::string, double> calculate(std::shared_ptr<boost::circular_buffer<OHLC>> bars) {
    std::map<std::string, double> values;
    if (period <= bars->size()) {
      std::vector<OHLC> period_bars(bars->end() - period, bars->end());
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

  std::pair<double, double> welfords_algorithm(std::vector<OHLC> bars) {
    int n = 0;
    double mean = 0;
    double M2 = 0;

    for (auto x : bars) {
      n += 1;
      double delta = x.close - mean;
      mean += delta / n;
      M2 += delta * (x.close - mean);
    }

    return std::make_pair(mean, M2 / (n - 1));
  }

  double sd;
};