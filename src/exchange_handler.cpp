#include "../include/exchange_handler.h"

using std::cout;                   using std::endl;
using std::cin;                    using std::remove;
using std::function;               using std::shared_ptr;
using std::string;                 using std::ofstream;
using std::chrono::seconds;        using std::this_thread::sleep_for;
using std::mutex;                  using std::vector;
using std::lock_guard;             using std::thread;
using std::vector;                 using std::map;
using std::chrono::minutes;        using std::bind;
using std::to_string;              using std::ostringstream;
using std::make_shared;            using std::make_unique;
using namespace std::placeholders;

BitcoinTrader::BitcoinTrader(shared_ptr<Config> config) :
  config(config),
  trading_log(new Log((*config)["trading_log"], config)),
  exchange_log(new Log((*config)["exchange_log"], config)),
  done(false),
  running_threads(),
  tick(),
  mktdata(),
  execution_lock(),
  strategies(),
  received_userinfo(false),
  received_a_tick(false) {
   
  // create and add the strategies
  auto strategy_a = make_shared<SMACrossover>("SMACrossover",
        // long callback
        [&]() {
          trading_log->output("LONGING");
        },
        // short callback
        [&]() {
          trading_log->output("SHORTING");
        });
  strategies.push_back(strategy_a);

  for (auto strategy : strategies) {
    // if we do not have a mktdata object for this period
    if (mktdata.count(strategy->period) == 0) {
      // create a mktdata object with the period the strategy uses
      mktdata[strategy->period] = make_shared<MktData>(strategy->period);
    }
    // tell the mktdata object about the strategy
    mktdata[strategy->period]->add_strategy(strategy);
  }
}

BitcoinTrader::~BitcoinTrader() {
  done = true;
  for (auto t : running_threads)
    if (t && t->joinable())
      t->join();
}

void BitcoinTrader::cancel_order(std::string order_id) {
  ostringstream os;
  os << "CANCELLING LIMIT ORDER " << order_id;
  trading_log->output(os.str());

  exchange->cancel_order(order_id);
}

void BitcoinTrader::start() {
  exchange = make_shared<OKCoin>(exchange_log, config);
  setup_exchange_callbacks();
  exchange->start();
  check_connection();
}

void BitcoinTrader::check_connection() {
  running_threads.push_back(std::make_shared<thread>(
    [&]() {
      // give some time for everything to start up
      sleep_for(seconds(10));
      while (!done) {
        sleep_for(seconds(1));
        if (exchange &&
            (((timestamp_now() - exchange->ts_since_last) > minutes(1)) ||
             (exchange->reconnect == true))) {
          exchange_log->output("RECONNECTING TO " + exchange->name);
          exchange = make_shared<OKCoin>(exchange_log, config);
          setup_exchange_callbacks();
          exchange->start();
        }
      }
    }
  ));
}

void BitcoinTrader::fetch_userinfo() {
  running_threads.push_back(make_shared<thread>(
    [&]() {
      while (!done) {
        // stop checking when we are reconnecting
        if (exchange->reconnect != true)
          exchange->userinfo();
        sleep_for(seconds(5));
      }
    }
  ));
}

void BitcoinTrader::handle_stops() {
  shared_ptr<Stop> triggered_stop;
  for (auto stop : stops) {
    if (stop->trigger(tick))
      triggered_stop = stop;
  }
  if (triggered_stop) {
    trading_log->output(triggered_stop->action());
    stops.clear();
    exchange->cancel_order(current_limit);
    if (triggered_stop->direction == "long")
      market_sell(user_btc);
    else
      market_buy(user_btc);
  }
}

void BitcoinTrader::setup_exchange_callbacks() {
  exchange->set_open_callback(function<void()>(
    [&]() {
      received_userinfo = false;
      exchange->subscribe_to_ticker();
    }
  ));
  exchange->set_userinfo_callback(function<void(double, double)>(
    [&](double btc, double cny) {
      user_btc = btc;
      user_cny = cny;
      if (!received_userinfo) {
        for (auto m : mktdata)
          exchange->subscribe_to_OHLC(m.second->period);
      }
      received_userinfo = true;
    }
  ));
  exchange->set_ticker_callback(function<void(long, double, double, double)>(
    [&](long timestamp, double last, double bid, double ask) {
      tick.timestamp = timestamp;
      tick.last = last;
      tick.bid = bid;
      tick.ask = ask;

      if (!received_a_tick)
        fetch_userinfo();

      handle_stops();
      received_a_tick = true;
    }
  ));
  exchange->set_OHLC_callback(function<void(minutes, long, double, double, double, double, double)>(
    [&](minutes period, long timestamp, double open, double high,
      double low, double close, double volume) {

      shared_ptr<OHLC> bar(new OHLC(timestamp, open, high,
            low, close, volume));

      mktdata[period]->add(bar);
    }
  ));
}
