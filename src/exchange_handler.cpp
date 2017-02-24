#include "../include/exchange_handler.h"

using std::cout;                   using std::endl;
using std::cin;                    using std::remove;
using std::function;               using std::shared_ptr;
using std::string;                 using std::ofstream;
using std::this_thread::sleep_for; using std::floor;
using std::mutex;                  using std::vector;
using std::lock_guard;             using std::thread;
using std::vector;                 using std::map;
using std::chrono::minutes;        using std::bind;
using std::to_string;              using std::ostringstream;
using std::make_shared;            using std::make_unique;
using namespace std::placeholders; using std::to_string;
using namespace std::chrono_literals;
using std::accumulate;

// REQUIRED TO EXPLICITLY ADD EACH EXCHANGE HERE
vector<shared_ptr<ExchangeHandler>> BitcoinTrader::exchange_metas() {
  return { okcoin_futs_h, okcoin_spot_h };
}

BitcoinTrader::BitcoinTrader(shared_ptr<Config> config) :
  okcoin_futs_h(
      make_shared<OKCoinFutsHandler>("OKCoinFuts",
                                     make_shared<Log>((*config)["okcoin_futs_log"]),
                                     config,
                                     OKCoinFuts::ContractType::Weekly)
  ),
  okcoin_spot_h(
      make_shared<OKCoinSpotHandler>("OKCoinSpot",
                                     make_shared<Log>((*config)["okcoin_spot_log"]),
                                                      config)
  ),
  config(config),
  trading_log(new Log((*config)["trading_log"], config)),
  done(false)
{
  // create the strategies
  strategies.push_back(make_shared<SMACrossover>("SMACrossover"));

  // we're using OKCoin Futs for our basket of strategies
  for (auto strategy : strategies) {
    // if we do not have a mktdata object for this period
    if (okcoin_futs_h->mktdata.count(strategy->period) == 0) {
      // create a mktdata object with the period the strategy uses
      okcoin_futs_h->mktdata[strategy->period] = make_shared<MktData>(strategy->period);
    }
    // tell the mktdata object about the strategy
    okcoin_futs_h->mktdata[strategy->period]->add_strategy(strategy);
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
  for (auto exchange_meta : exchange_metas()) {
    lock_guard<mutex> l(exchange_meta->reconnect);

    os << exchange_meta->exchange->status();
    Ticker tick = exchange_meta->tick.get();
    os << "bid: " << tick.bid << ", ask: " << tick.ask << std::endl;
    os << exchange_meta->print_userinfo();
    for (auto &m : exchange_meta->mktdata) {
      os << "MktData with period: " <<  m.second->period.count();
      os << ", size: " << m.second->bars->size() << endl;
      os << "last: ";
      os << m.second->bars->back().to_string() << endl;
    }
  }
  return os.str();
}

void BitcoinTrader::start() {
  for (auto exchange : exchange_metas())
    exchange->set_up_and_start();

  // check connection and reconnect if down on another thread
  check_connection();

  // start fetching userinfo on another thread
  fetch_userinfo();

  // wait until OKCoinFuts userinfo and ticks are fetched,
  // and subscribed to market data
  auto can_open_positions = [&]() {
    // we can open positions if
    bool can = true;
    // we're subscribed to every OHLC period
    for (auto &m : okcoin_futs_h->mktdata) {
      lock_guard<mutex> l(okcoin_futs_h->reconnect);
      can = can && okcoin_futs_h->okcoin_futs->subscribed_to_OHLC(m.first);
    }
    // and every strategy has a signal that's been set
    can = can && accumulate(strategies.begin(), strategies.end(), true,
                            [](bool a, shared_ptr<Strategy> b) { return a && b->signal.has_been_set(); });
    // and we have userinfo and a tick set
    can = can &&
        okcoin_futs_h->user_info.has_been_set() &&
        okcoin_futs_h->tick.has_been_set();
    return can;
  };
  // this can block forever, because there's no reason to manage positions if can_open_positions is never true
  check_until(can_open_positions);

  // manage positions on another thread
  position_management();
}

void BitcoinTrader::position_management() {
  auto position_thread = [&]() {
    while (!done && okcoin_futs_h->okcoin_futs->connected()) {
      {
        lock_guard<mutex> l(okcoin_futs_h->reconnect);
        double blended_signal = blend_signals();
        manage_positions(blended_signal);
      }
      sleep_for(5s);
    }
  };
  running_threads.push_back(make_shared<thread>(position_thread));
}

void BitcoinTrader::check_connection() {
  auto connection_thread = [&]() {
    bool warm_up = true;
    while (!done) {
      // give some time for everything to start up after we reconnect
      if (warm_up) {
        sleep_for(10s);
        warm_up = false;
      }
      for (auto exchange : exchange_metas()) {
        if (exchange->exchange &&
            // if the time since the last message received is > 1min
            (((timestamp_now() - exchange->exchange->ts_since_last) > 1min) ||
             // if the websocket has closed
             !exchange->exchange->connected())) {
          exchange->log->output("RECONNECTING TO " + exchange->name);
          exchange->set_up_and_start();
          warm_up = true;
        }
      }
      // check to reconnect every second
      sleep_for(1s);
    }
  };
  running_threads.push_back(make_shared<thread>(connection_thread));
}

void BitcoinTrader::fetch_userinfo() {
  auto userinfo_thread = [&]() {
    while (!done) {
      for (auto exchange : exchange_metas()) {
        lock_guard<mutex> l(exchange->reconnect);

        if (exchange->exchange->connected())
          exchange->exchange->userinfo();
      }
      sleep_for(1s);
    }
  };
  running_threads.push_back(make_shared<thread>(userinfo_thread));
}
