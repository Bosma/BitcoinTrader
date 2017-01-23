#include "../include/exchange_handler.h"

using std::shared_ptr;
using std::accumulate;

double BitcoinTrader::blend_signals() {
  // average the signals
  double signal_sum = accumulate(strategies.begin(), strategies.end(), 0,
      [](double a, shared_ptr<Strategy> b) { return a + b->signal; });
  return signal_sum / strategies.size();
}

void BitcoinTrader::manage_positions(double signal) {
  auto info = okcoin_futs.user_info.get();
  std::cout << "manage_positions: " << info.equity << " equity with signal: " << signal << std::endl;
}
