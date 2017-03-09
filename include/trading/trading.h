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

#include "utilities/exchange_utils.h"

using buffer = boost::circular_buffer<OHLC>;

class Indicator {
public:
  Indicator(std::string name, int period) : name(name), period(period) { }
  virtual std::map<std::string, IndicatorValue> calculate(const buffer&) = 0;

  std::string name;
  int period;
};

class MovingAverage : public Indicator {
public:
  MovingAverage(std::string, int);

  std::map<std::string, IndicatorValue> calculate(const buffer&) override;
};

class BollingerBands : public Indicator {
public:
  BollingerBands(std::string, int period = 20, double sd = 2);

  std::map<std::string, IndicatorValue> calculate(const buffer&) override;

  std::pair<double, double> welfords_algorithm(const buffer::const_iterator, const buffer::const_iterator);

  double sd;
};