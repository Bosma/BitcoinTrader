#include "handlers/okcoin_futs_handler.h"

using std::lock_guard;
using std::mutex;
using std::shared_ptr;
using std::make_shared;
using std::string;
using std::chrono::minutes;
using std::to_string;
using std::this_thread::sleep_for;
using namespace std::chrono_literals;

OKCoinFutsHandler::OKCoinFutsHandler(string name, shared_ptr<Config> config, string exchange_log_key, string trading_log_key, string execution_log_key, OKCoinFuts::ContractType contract_type) :
    ExchangeHandler(name, config, exchange_log_key, trading_log_key, execution_log_key),
    contract_type(contract_type) {
}

void OKCoinFutsHandler::set_up_and_start() {
  // let infinitely long check_untils know that we're reconnecting
  // so they release their hold on the reconnect lock
  cancel_checking = true;
  lock_guard<mutex> l(reconnect);
  // we have the lock, so we're safe to delete the exchange pointers
  // and they can start waiting again
  cancel_checking = false;

  tick.clear();
  depth.clear();

  okcoin_futs = std::make_shared<OKCoinFuts>(name, contract_type, exchange_log, config);
  exchange = okcoin_futs;

  // set the callbacks the exchange will use
  auto open_callback = [&]() {
    std::lock_guard<std::mutex> g(reconnect);
    // start receiving ticks and block until one is received (or we're told to stop)
    okcoin_futs->subscribe_to_ticker();
    check_until([&]() { return tick.has_been_set() || cancel_checking; });

    // do the same for depth
    okcoin_futs->subscribe_to_depth();
    check_until([&]() { return depth.has_been_set() || cancel_checking; });

    // backfill and subscribe to each market data
    for (auto &m : mktdata) {
      auto OHLC_is_fetched = [&]() {
        trading_log->output("BACKFILLING OKCOIN FUTS " + to_string(m.first.count()) + "m BARS");
        bool success = okcoin_futs->backfill_OHLC(m.second.period, m.second.bars.capacity());
        if (success)
          trading_log->output("FINISHED BACKFILLING OKCOIN FUTS " + to_string(m.first.count()) + "m BARS");
        else
          exchange_log->output("FAILED TO BACKFILL " + to_string(m.second.period.count()) + "m BARS FOR OKCOIN FUTS, TRYING AGAIN.");
        return success;
      };
      if (!check_until(OHLC_is_fetched, timestamp_now() + 1min, 10s))
        exchange_log->output("FAILED TO BACKFILL " + to_string(m.second.period.count()) + "m BARS FOR OKCOIN FUTS, GIVING UP.");
      okcoin_futs->subscribe_to_OHLC(m.second.period);
    }
  };
  okcoin_futs->set_open_callback(open_callback);

  auto OHLC_callback = [&](minutes period, const OHLC& bar) {
    // do not need to check for existence of period in map, since it is designed to exist
    // if there somehow isn't, this is called inside a try
    mktdata.at(period).add(bar);
  };
  okcoin_futs->set_OHLC_callback(OHLC_callback);

  auto ticker_callback = [&](const Ticker& new_tick) {
    for (auto &m : mktdata) {
      m.second.add(new_tick);
    }
    tick.set(new_tick);
  };
  okcoin_futs->set_ticker_callback(ticker_callback);

  auto depth_callback = [&](const Depth& new_depth) {
    depth.set(new_depth);
  };
  okcoin_futs->set_depth_callback(depth_callback);

  // start the exchange
  okcoin_futs->start();
}