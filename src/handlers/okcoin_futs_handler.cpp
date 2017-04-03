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

OKCoinFutsHandler::OKCoinFutsHandler(string name, shared_ptr<Config> config, string exchange_log_key,
                                     string trading_log_key, OKCoinFuts::ContractType contract_type) :
    ExchangeHandler(name, config, exchange_log_key, trading_log_key),
    execution_csv(name + "_algorithms.csv", {"algorithm",
                                             "contract_type",
                                             "start_time",
                                             "end_time",
                                             "contract_amount",
                                             "limit_price",
                                             "filled_amount",
                                             "average_price",
                                             "posting_depth",
                                             "expire_depth"}),
    contract_type(contract_type) {
}

void OKCoinFutsHandler::log_execution(string algorithm,
                                      OKCoinFuts::OrderType type,
                                      timestamp_t t1,
                                      OKCoinFuts::OrderInfo &final_info,
                                      double amount, double limit_price,
                                      Depth &d1,
                                      Depth &d2) {
  execution_csv.row({
                        algorithm,
                        to_string(static_cast<int>(type)),
                        to_string(t1.time_since_epoch().count()),
                        to_string(final_info.timestamp.time_since_epoch().count()),
                        to_string(amount),
                        to_string(limit_price),
                        to_string(final_info.filled_amount),
                        to_string(final_info.avg_price),
                        d1.to_string(),
                        d2.to_string()
                    });
}

void OKCoinFutsHandler::set_up_and_start() {
  okcoin_futs = std::make_shared<OKCoinFuts>(name, contract_type, exchange_log, config);
  exchange = okcoin_futs;

  // set the callbacks the exchange will use
  auto open_callback = [&]() {
    exchange_log->output("OKCoinFutsHandler DEBUG: STARTED OPEN CALLBACK");

    okcoin_futs->subscribe_to_ticker();
    okcoin_futs->subscribe_to_depth();

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
    exchange_log->output("OKCoinFutsHandler DEBUG: DONE OPEN CALLBACK");
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

void OKCoinFutsHandler::reconnect_exchange() {
  exchange_log->output("RECONNECTING TO " + name);

  tick.clear();
  depth.clear();

  okcoin_futs->reconnect();
}