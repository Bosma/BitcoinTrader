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
  strategies() {
   
  // create the strategies
  strategies.push_back(make_shared<SMACrossover>("SMACrossover"));

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
    os << "last: " << m.second->bars->back().to_string() << endl;
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
  exchange = make_shared<OKCoinFuts>(OKCoinFuts::Weekly, exchange_log, config);
  setup_exchange_callbacks();
  exchange->start();

  // check connection and reconnect if down on another thread
  // this has its own sleep so can happen before the below sleep
  check_connection();

  // give it some time to warm up
  sleep_for(seconds(5));

  // start fetching userinfo on another thread
  fetch_userinfo();

  // give it time to fetch userinfo
  sleep_for(seconds(5));

  // manage positions on another thread
  position_management();
}

void BitcoinTrader::position_management() {
  auto position_thread = [&]() {
    while (!done) {
      double blended_signal = blend_signals();
      manage_positions(blended_signal);
      sleep_for(milliseconds(100));
    }
  };
  running_threads.push_back(make_shared<thread>(position_thread));
}

void BitcoinTrader::check_connection() {
  auto connection_thread = [&]() {
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
           exchange->reconnect)) {
        exchange_log->output("RECONNECTING TO " + exchange->name);
        exchange = make_shared<OKCoinSpot>(OKCoinFuts::Weekly, exchange_log, config);
        setup_exchange_callbacks();
        exchange->start();
        warm_up = true;
      }
    }
  };
  running_threads.push_back(make_shared<thread>(connection_thread));
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
  auto userinfo_thread = [&]() {
    while (!done) {
      exchange->set_userinfo_callback([&](UserInfo uinfo) {
        set_userinfo(uinfo);
      });
      exchange->userinfo();
      sleep_for(seconds(1));
    }
  };
  running_threads.push_back(make_shared<thread>(userinfo_thread));
}

void BitcoinTrader::setup_exchange_callbacks() {
  auto open_callback = [&]() {
    exchange->subscribe_to_ticker();
    // backfill and subscribe to each market data
    for (auto m : mktdata) {
      exchange->backfill_OHLC(m.second->period, m.second->bars->capacity());
      exchange->subscribe_to_OHLC(m.second->period);
    }
  };
  exchange->set_open_callback(open_callback);

  auto OHLC_callback = [&](minutes period, OHLC bar) {
    lock_guard<mutex> lock(OHLC_lock);
    mktdata[period]->add(bar);
  };
  exchange->set_OHLC_callback(OHLC_callback);

  auto ticker_callback = [&](Ticker new_tick) {
    lock_guard<mutex> lock(ticker_lock);
    tick = new_tick;
    handle_stops();
  };
  exchange->set_ticker_callback(ticker_callback);
}
