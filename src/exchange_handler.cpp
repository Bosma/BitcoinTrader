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
using namespace std::placeholders; using std::to_string;
using std::floor;                  using std::chrono::milliseconds;

BitcoinTrader::BitcoinTrader(shared_ptr<Config> config) :
  config(config),
  trading_log(new Log((*config)["trading_log"], config)),
  exchange_log(new Log((*config)["exchange_log"], config)),
  done(false),
  running_threads(),
  tick(),
  mktdata(),
  strategies(),
  received_a_tick(false),
  fetching_userinfo_already(false) {
   
  // create the strategies
  strategies.push_back(make_shared<SMACrossover>("SMACrossover",
    // long callback
    [&]() {
      trading_log->output("LONGING");
      close_short_then_long(2);
    },
    // short callback
    [&]() {
      trading_log->output("SHORTING");
      close_long_then_short(2);
    }
  ));

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

string BitcoinTrader::status() {
  ostringstream os;
  os << exchange->status();
  for (auto m : mktdata) {
    os << "MktData with period: " <<  m.second->period.count();
    os << ", size: " << m.second->bars->size() << endl;
    os << "last: " << m.second->bars->back()->to_string() << endl;
  }
  return os.str();
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
  running_threads.push_back(make_shared<thread>(
    [&]() {
      bool warm_up = true;
      while (!done) {
        // give some time for everything to start up
        // the first time and times after we reconnect
        if (warm_up) {
          sleep_for(seconds(10));
          warm_up = false;
        }
        // check to reconnect every second
        sleep_for(seconds(1));
        if (exchange &&
            // if the time since the last message received is > 1min
            (((timestamp_now() - exchange->ts_since_last) > minutes(1)) ||
             // if the websocket has closed
             // this may cause too many false reconnects due to random
             // okcoin fuckery
             exchange->reconnect)) {
          exchange_log->output("RECONNECTING TO " + exchange->name);
          exchange = make_shared<OKCoin>(exchange_log, config);
          setup_exchange_callbacks();
          exchange->start();
          warm_up = true;
        }
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
    // unfinished
  }
}

void BitcoinTrader::fetch_userinfo() {
  fetching_userinfo_already = true;
  running_threads.push_back(make_shared<thread>([&]() {
    while (!done) {
      exchange->set_userinfo_callback([&](Exchange::UserInfo uinfo) {
        set_userinfo(uinfo);
        if (subscribe)
          exchange->subscribe_to_ticker();
        subscribe = false;
      });
      exchange->userinfo();
      sleep_for(seconds(1));
    }
  }));
}

void BitcoinTrader::setup_exchange_callbacks() {
  exchange->set_open_callback(function<void()>(
    [&]() {
      subscribe = true;
      if (!fetching_userinfo_already)
        fetch_userinfo();
    }
  ));
  exchange->set_OHLC_callback(function<void(minutes, long, double, double, double, double, double, bool)>(
    [&](minutes period, long timestamp, double open, double high,
      double low, double close, double volume, bool backfilling) {

      shared_ptr<OHLC> bar(new OHLC(timestamp, open, high,
            low, close, volume));

      mktdata[period]->add(bar, backfilling);
    }
  ));
  exchange->set_ticker_callback(function<void(long, double, double, double)>(
    [&](long timestamp, double last, double bid, double ask) {
      tick.timestamp = timestamp;
      tick.last = last;
      tick.bid = bid;
      tick.ask = ask;

      if (!received_a_tick) {
        received_a_tick = true;
        for (auto m : mktdata) {
          // before we subscribe, backfill the data
          exchange->backfill_OHLC(m.second->period, m.second->bars->capacity());
          exchange->subscribe_to_OHLC(m.second->period);
        }
      }

      handle_stops();
    }
  ));
}
