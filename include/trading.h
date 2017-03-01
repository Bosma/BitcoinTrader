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
  virtual std::map<std::string, IndicatorValue> calculate(std::shared_ptr<boost::circular_buffer<OHLC>>) = 0;
  std::string name;
  int period;
};

class MovingAverage : public Indicator {
public:
  MovingAverage(std::string name, int period) :
      Indicator(name, period) { }

  std::map<std::string, IndicatorValue> calculate(std::shared_ptr<boost::circular_buffer<OHLC>> bars) {
    std::map<std::string, IndicatorValue> values;
    IndicatorValue mavg;
    if (period <= bars->size()) {
      std::vector<OHLC> period_bars(bars->end() - period, bars->end());

      double sum = 0;
      for (auto bar : period_bars)
        sum += bar.close;

      mavg.set(sum / period);
    }
    values["mavg"] = mavg;
    return values;
  }
};

class BollingerBands : public Indicator {
public:
  BollingerBands(std::string name, int period = 20, double sd = 2) :
      Indicator(name, period), sd(sd) { }

  std::map<std::string, IndicatorValue> calculate(std::shared_ptr<boost::circular_buffer<OHLC>> bars) {
    std::map<std::string, IndicatorValue> values;
    IndicatorValue mavg, up, dn, bw;
    if (period <= bars->size()) {
      std::vector<OHLC> period_bars(bars->end() - period, bars->end());
      auto welford = welfords_algorithm(period_bars);

      double moving_average = welford.first;
      double std = sqrt(welford.second);

      mavg.set(moving_average);
      up.set(moving_average + (std * sd));
      dn.set(moving_average - (std * sd));
      bw.set(values["up"].get() - values["dn"].get());
    }
    values["mavg"] = mavg;
    values["up"] = up;
    values["dn"] = dn;
    values["bw"] = bw;
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