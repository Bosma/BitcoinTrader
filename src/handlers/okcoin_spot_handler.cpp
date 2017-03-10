#include "handlers/okcoin_spot_handler.h"

using std::lock_guard;
using std::mutex;
using std::shared_ptr;
using std::string;

OKCoinSpotHandler::OKCoinSpotHandler(string name, shared_ptr<Log> log, shared_ptr<Config> config) :
    ExchangeHandler(name, log, config) {
}

void OKCoinSpotHandler::set_up_and_start() {
  cancel_checking = true;
  std::lock_guard<std::mutex> l(reconnect);
  cancel_checking = false;

  tick.clear();

  okcoin_spot = std::make_shared<OKCoinSpot>(name, log, config);
  exchange = okcoin_spot;

  auto open_callback = [&]() {
    std::lock_guard<std::mutex> g(reconnect);
    okcoin_spot->subscribe_to_ticker();
  };
  okcoin_spot->set_open_callback(open_callback);

  auto ticker_callback = [&](const Ticker& new_tick) {
    tick.set(new_tick);
  };
  okcoin_spot->set_ticker_callback(ticker_callback);

  okcoin_spot->start();
}