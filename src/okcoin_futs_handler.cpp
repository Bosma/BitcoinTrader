#include "../include/okcoin_futs_handler.h"

using std::lock_guard;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::chrono::minutes;

OKCoinFutsHandler::OKCoinFutsHandler(string name, shared_ptr<Log> log, shared_ptr<Config> config, OKCoinFuts::ContractType contract_type) :
    ExchangeHandler(name, log, config), contract_type(contract_type) {
}

void OKCoinFutsHandler::set_up_and_start() {
  // let infinitely long check_untils know that we're reconnecting
  // so they release their hold on the reconnect lock
  cancel_checking = true;
  lock_guard<mutex> l(reconnect);
  // we have the lock, so we're safe to delete the exchange pointers
  // and they can start waiting again
  cancel_checking = false;

  user_info.clear();
  tick.clear();

  okcoin_futs = std::make_shared<OKCoinFuts>(name, contract_type, log, config);
  exchange = okcoin_futs;

  // set the callbacks the exchange will use
  auto open_callback = [&]() {
    std::lock_guard<std::mutex> g(reconnect);
    // start receiving ticks and block until one is received (or we're told to stop)
    okcoin_futs->subscribe_to_ticker();
    check_until([&]() { return tick.has_been_set() || cancel_checking; });

    // backfill and subscribe to each market data
    for (auto &m : mktdata) {
      okcoin_futs->backfill_OHLC(m.second->period, m.second->bars->capacity());
      okcoin_futs->subscribe_to_OHLC(m.second->period);
    }
  };
  okcoin_futs->set_open_callback(open_callback);

  auto OHLC_callback = [&](minutes period, OHLC bar) {
    mktdata[period]->add(bar);
  };
  okcoin_futs->set_OHLC_callback(OHLC_callback);

  auto ticker_callback = [&](const Ticker new_tick) {
    for (auto &m : mktdata) {
      m.second->add(new_tick);
    }
    tick.set(new_tick);
  };
  okcoin_futs->set_ticker_callback(ticker_callback);

  auto userinfo_callback = [&](OKCoinFuts::UserInfo info) {
    user_info.set(info);
  };
  okcoin_futs->set_userinfo_callback(userinfo_callback);

  // start the exchange
  okcoin_futs->start();
}

string OKCoinFutsHandler::print_userinfo() {
  return user_info.get().to_string();
}