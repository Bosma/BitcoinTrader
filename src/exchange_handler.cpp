#include "../include/exchange_handler.h"

using std::cout;            using std::endl;
using std::cin;             using std::remove;
using std::function;        using std::shared_ptr;
using std::string;          using std::ofstream;
using std::chrono::seconds; using std::this_thread::sleep_for;
using std::mutex;           using std::vector;
using std::lock_guard;      using std::thread;
using std::vector;          using std::map;
using std::chrono::minutes; using std::bind;
using std::to_string;       using std::ostringstream;
using std::make_shared; using std::make_unique;
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
    if (t.second && t.second->joinable())
      t.second->join();
}

void BitcoinTrader::buy(double amount) {
  if (trading_log) {
    ostringstream os;
    os << "BUYING " << amount << " BTC @ " << tick.ask;
    trading_log->output(os.str());
  }

  exchange->market_buy(amount);
}

void BitcoinTrader::sell(double amount) {
  if (trading_log) {
    ostringstream os;
    os << "SELLING " << amount << " BTC @ " << tick.bid;
    for (auto stop : stops)
      os << ", " << stop->to_string();
    trading_log->output(os.str());
  }

  exchange->market_sell(amount);
}

void BitcoinTrader::sell_all() {
  sell(user_btc);
}

void BitcoinTrader::limit_buy(double amount, double price, seconds cancel_time) {
  if (trading_log) {
    ostringstream os;
    os << "LIMIT BUYING " << amount << " BTC @ " << price;
    trading_log->output(os.str());
  }

  set_limit_callbacks(cancel_time);
  exchange->limit_buy(amount, price);
}

void BitcoinTrader::limit_sell(double amount, double price, seconds cancel_time) {
  if (trading_log) {
    ostringstream os;
    os << "LIMIT SELLING " << amount << " BTC @ " << price;
    trading_log->output(os.str());
  }

  set_limit_callbacks(cancel_time);
  exchange->limit_sell(amount, price);
}

void BitcoinTrader::cancel_order(std::string order_id) {
  if (trading_log) {
    ostringstream os;
    os << "CANCELLING LIMIT ORDER " << order_id;
    trading_log->output(os.str());
  }
}

void BitcoinTrader::start() {
  exchange = make_shared<OKCoin>(exchange_log, config);
  setup_exchange_callbacks();
  exchange->start();
  check_connection();
}

void BitcoinTrader::check_connection() {
  auto connection_checker = std::make_shared<thread>(
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
  );
  running_threads["connection_checker"] = connection_checker;
}

void BitcoinTrader::fetch_userinfo() {
  auto userinfo_fetcher = make_shared<thread>(
    [&]() {
      while (!done) {
        // stop checking when we are reconnecting
        if (exchange->reconnect != true)
          exchange->userinfo();
        sleep_for(seconds(5));
      }
    }
  );
  running_threads["userinfo_fetcher"] = userinfo_fetcher;
}

void BitcoinTrader::handle_stops() {
  shared_ptr<Stop> triggered_stop;
  for (auto stop : stops) {
    if (stop->trigger(tick))
      triggered_stop = stop;
  }
  if (triggered_stop) {
    if (trading_log)
      trading_log->output(triggered_stop->action());
    stops.clear();
    exchange->cancel_order(current_limit);
    if (triggered_stop->direction == "long") {
      sell(user_btc);
    }
    else {
      buy(user_btc * tick.ask);
    }
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

void BitcoinTrader::set_takeprofit_callbacks() {
  exchange->set_trade_callback(function<void(string)>([&](string order_id) {
    current_limit = order_id;
  }));
}

void BitcoinTrader::set_limit_callbacks(seconds limit) {
  execution_lock.lock();

  exchange->set_trade_callback(function<void(string)>(
    [&](string order_id) {
      current_limit = order_id;
      auto limit_checker = make_shared<thread>([&]() {
        filled_amount = 0;
        auto start_time = timestamp_now();
        done_limit_check = false;
        // until we are done,
        // we fully filled for the entire amount
        // or some seconds have passed
        while (!done && !done_limit_check &&
            timestamp_now() - start_time < limit) {
          // fetch the orderinfo every 2 seconds
          exchange->orderinfo(current_limit);
          sleep_for(seconds(2));
        }

        // given the limit enough time, cancel it
        exchange->cancel_order(current_limit);

        execution_lock.unlock();
        
        if (filled_amount != 0) {
          // we can now add the stops
          stops.insert(stops.end(),
              pending_stops.begin(), pending_stops.end());
          // and our take profit limit
          set_takeprofit_callbacks();
          limit_sell(filled_amount, tp_limit, limit);

          if (trading_log)
            trading_log->output("FILLED FOR " + to_string(filled_amount) + " BTC");
        }
        else {
          if (trading_log)
            trading_log->output("NOT FILLED IN TIME");
        }

        pending_stops.clear();
      });
      running_threads["limit_checker"] = limit_checker;
    }
  ));
  exchange->set_orderinfo_callback(function<void(OrderInfo)>(
    [&](OrderInfo orderinfo) {
      cout << orderinfo.to_string() << endl;
      // early stopping if we fill for the entire amount
      if (orderinfo.amount == orderinfo.filled_amount)
        done_limit_check = true;
      filled_amount = orderinfo.filled_amount;
    }
  ));
}
