#include "trading/trading.h"

/******************/
/* MOVING AVERAGE */
/******************/

MovingAverage::MovingAverage(std::string name, int period) :
    Indicator(name, period) { }

std::map<std::string, IndicatorValue> MovingAverage::calculate(const buffer& bars) {
  std::map<std::string, IndicatorValue> values;
  IndicatorValue mavg;
  if (period <= bars.size()) {
    double sum = accumulate(bars.end() - period,
                            bars.end(),
                            0,
                            [](double a, OHLC b) { return a + b.close; });
    mavg.set(sum / period);
  }
  values["mavg"] = mavg;
  return values;
}

/*******************/
/* BOLLINGER BANDS */
/*******************/

BollingerBands::BollingerBands(std::string name, int period, double sd) :
    Indicator(name, period), sd(sd) { }

std::map<std::string, IndicatorValue> BollingerBands::calculate(const buffer& bars) {
  std::map<std::string, IndicatorValue> values;
  IndicatorValue mavg, up, dn, bw;
  if (period <= bars.size()) {
    auto welford = welfords_algorithm(bars.end() - period,
                                      bars.end());

    double moving_average = welford.first;
    double std = sqrt(welford.second);

    mavg.set(moving_average);
    up.set(moving_average + (std * sd));
    dn.set(moving_average - (std * sd));
    bw.set(2 * std * sd);
  }
  values["mavg"] = mavg;
  values["up"] = up;
  values["dn"] = dn;
  values["bw"] = bw;
  return values;
}

std::pair<double, double> BollingerBands::welfords_algorithm(const buffer::const_iterator begin, const buffer::const_iterator end) {
  int n = 0;
  double mean = 0;
  double M2 = 0;

  for (auto i = begin; i != end; i++) {
    n += 1;
    double delta = (*i).close - mean;
    mean += delta / n;
    M2 += delta * ((*i).close - mean);
  }

  return std::make_pair(mean, M2 / (n - 1));
}