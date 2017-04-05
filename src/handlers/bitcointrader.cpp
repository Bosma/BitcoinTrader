#include "handlers/bitcointrader.h"

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
using std::piecewise_construct;    using std::forward_as_tuple;
using namespace std::chrono_literals;
using std::accumulate;

BitcoinTrader::BitcoinTrader(shared_ptr<Config> config) :
    config(config),
    done(false)
{
  user_specifications();

  // for each exchange handler
  for (auto handler : exchange_handlers) {
    // for every strategy that this exchange handler manages
    for (auto strategy : handler->signal_strategies) {
      // if we do not have a mktdata object for this period
      if (handler->mktdata.count(strategy->period) == 0) {
        // create a mktdata object with the period the strategy uses
        handler->mktdata.emplace(piecewise_construct,
                                    forward_as_tuple(strategy->period),
                                    forward_as_tuple(strategy->period, 2000, handler->trading_log));
      }
      // tell the mktdata object about the strategy
      handler->mktdata.at(strategy->period).add_strategy(strategy);
    }
  }

}

BitcoinTrader::~BitcoinTrader() {
  done = true;
  for (auto& t : running_threads)
    if (t.joinable())
      t.join();
}

string BitcoinTrader::status() {
  ostringstream os;
  for (auto i = exchange_handlers.begin(); i != exchange_handlers.end(); i++) {
    os << (*i)->exchange->status();
    Ticker tick = (*i)->tick.get();
    os << "Bid: " << tick.bid << ", Ask: " << tick.ask << endl;
    for (auto &m : (*i)->mktdata) {
      os << m.second.status() << endl;
    }
    for (auto s : (*i)->strategies()) {
      os << s->status() << endl;
    }
    os << "blended signal: " << blend_signals(*i) << std::endl;
    if (next(i) != exchange_handlers.end())
      os << endl;
  }
  return os.str();
}

void BitcoinTrader::print_bars() {
  for (auto handler : exchange_handlers) {
    for (auto& m : handler->mktdata) {
      string file_name = handler->name + "_" + to_string(m.first.count()) + "m.csv";
      CSV csv(file_name, m.second.bars.at(0).to_columns(), CSV::Mode::Overwrite);
      for (auto& bar : m.second.bars)
        csv.row(bar.to_csv());
      std::cout << "written " << file_name << std::endl;
    }
  }
}

void BitcoinTrader::start() {
  // check connection and reconnect if down on another thread
  manage_connections();

  // manage positions on another thread
  position_management();
}

void BitcoinTrader::position_management() {
  auto position_thread = [this](shared_ptr<ExchangeHandler> handler) {
    // wait until OKCoinFuts userinfo and ticks are fetched,
    // and subscribed to market data
    auto can_open_positions = [this, handler]() {
      // return right away if we're done
      if (done)
        return true;

      // we can't if there's no exchange object
      if (!handler->exchange)
        return false;

      // we can open positions if
      bool can = true;

      // we're subscribed to every OHLC period
      for (auto &m : handler->mktdata)
        can = can && handler->exchange->subscribed_to_OHLC(m.first);

      // every strategy has a set signal
      can = can && accumulate(handler->signal_strategies.begin(), handler->signal_strategies.end(), true,
                              [](bool a, shared_ptr<SignalStrategy> b) { return a && b->signal.has_been_set(); });

      // and we have a tick set
      can = can && handler->tick.has_been_set();

      // and we have the depth set
      can = can && handler->depth.has_been_set();

      return can;
    };
    // this can block forever, because there's no reason to manage positions if can_open_positions is never true
    check_until(can_open_positions);

    while (!done) {
      if (handler->exchange->connected()) {
        double blended_signal = blend_signals(handler);
        handler->manage_positions(blended_signal);
      }
      sleep_for(10s);
    }
  };

  for (auto handler : exchange_handlers) {
    // no need to manage positions for exchanges with no strategies
    if (!handler->strategies().empty())
      running_threads.emplace_back(position_thread, handler);
  }
}

void BitcoinTrader::manage_connections() {
  auto connection_thread = [this](shared_ptr<ExchangeHandler> handler) {
    handler->set_up_and_start();

    bool warm_up = true;
    auto warm_up_time(20s);

    while (!done) {
      // give some time for everything to start up after we reconnect
      if (warm_up) {
        handler->exchange_log->output("CONNECTION MANAGER SLEEPING FOR " + to_string(warm_up_time.count()) + "s BEFORE CHECKING CONNECTION");
        sleep_for(warm_up_time);
        handler->exchange_log->output("CONNECTION MANAGER DONE SLEEPING");
        warm_up = false;
      }

      // ping the exchange to let them know we are here
      handler->exchange->ping();

      // if the time since the last message received is > 1min
      if ((timestamp_now() - handler->exchange->ts_since_last.get() > 1min) ||
          // if the websocket has closed
          !handler->exchange->connected()) {
        handler->reconnect_exchange();
        warm_up = true;
        if (warm_up_time < 320s)
          warm_up_time *= 2;
      }
      // if we do not have to reconnect, reset the warm_up_time
      else {
        warm_up_time = 20s;
      }

      // check to reconnect every second
      sleep_for(1s);
    }
  };

  for (auto handler : exchange_handlers) {
    running_threads.emplace_back(connection_thread, handler);
  }
}
