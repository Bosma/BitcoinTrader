#include "../include/exchange_handler.h"

using std::string;                 using std::function;
using std::make_shared;            using std::thread;
using std::this_thread::sleep_for; using std::chrono::seconds;
using std::cout;                   using std::endl;
using std::to_string;              using std::ostringstream;

void BitcoinTrader::full_margin_long(double percent) {
  // borrowing MAX CNY
  string borrow_id = exchange->borrow(Currency::CNY, percent);

  exchange->set_userinfo_callback([&](double btc, double cny) {
      // after we know how much CNY we now have, market buy it all
      market_buy(floor(cny),
          [&](double avg_price, double amount, long date) {
            trading_log->output("BOUGHT " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");
          });
      });

  // need to find out how much CNY we have now
  exchange->userinfo();
}

void BitcoinTrader::full_margin_short(double percent) {
  // borrowing MAX BTC
  string borrow_id = exchange->borrow(Currency::BTC, percent);

  exchange->set_userinfo_callback([&](double btc, double cny) {
      // after we know how much BTC we now have, market sell it all
      market_sell(floor(btc),
          [&](double avg_price, double amount, long date) {
            trading_log->output("SOLD " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");
          });
      });

  // need to find out how much BTC we have now
  exchange->userinfo();
}

void BitcoinTrader::close_margin_short() {
  trading_log->output("CLOSING MARGIN SHORT");

  exchange->set_userinfo_callback([&](double btc, double cny) {
      market_buy(floor(cny),
          [&](double avg_price, double amount, long date) {
            if (date != 0)
              trading_log->output("BOUGHT " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");

            exchange->close_borrow(Currency::BTC);
          });
      });

  exchange->userinfo();
}

void BitcoinTrader::close_margin_long() {
  trading_log->output("CLOSING MARGIN LONG");

  exchange->set_userinfo_callback([&](double btc, double cny) {
      market_sell(floor(btc),
          [&](double avg_price, double amount, long date) {
            if (date != 0)
              trading_log->output("SOLD " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");

            exchange->close_borrow(Currency::CNY);
          });
      });

  exchange->userinfo();
}

void BitcoinTrader::limit_buy(double amount, double price, seconds cancel_time, function<void(double)> callback) {
  ostringstream os;
  os << "LIMIT BUYING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  limit_callback = callback;

  limit_algorithm(cancel_time);
  exchange->limit_buy(amount, price);
}

void BitcoinTrader::limit_sell(double amount, double price, seconds cancel_time, function<void(double)> callback) {
  ostringstream os;
  os << "LIMIT SELLING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  limit_callback = callback;

  limit_algorithm(cancel_time);
  exchange->limit_sell(amount, price);
}

void BitcoinTrader::limit_algorithm(seconds limit) {
  execution_lock.lock();

  exchange->set_trade_callback(function<void(string)>(
    [&](string order_id) {
      current_limit = order_id;
      running_threads.push_back(make_shared<thread>([&]() {
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
          trading_log->output("FILLED FOR " + to_string(filled_amount) + " BTC");
          if (limit_callback) {
            limit_callback(filled_amount);
            limit_callback = nullptr;
          }
        }
        else
          trading_log->output("NOT FILLED IN TIME");

        pending_stops.clear();
      }));
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

void BitcoinTrader::market_buy(double amount, function<void(double, double, long)> callback) {
  execution_lock.lock();

  market_callback = callback;

  ostringstream os;
  os << "MARKET BUYING " << amount << " CNY @ " << tick.ask;
  trading_log->output(os.str());
  
  // none of this is required if we don't have a callback
  if (market_callback) {
    exchange->set_orderinfo_callback(function<void(OrderInfo)>(
      [&](OrderInfo orderinfo) {
        market_callback(orderinfo.avg_price, orderinfo.filled_amount, orderinfo.create_date);
        market_callback = nullptr;
        execution_lock.unlock();
      }
    ));
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        if (order_id == "failed") {
          market_callback(0, 0, 0);
          market_callback = nullptr;
          execution_lock.unlock();
        }
        else
          exchange->orderinfo(order_id);
      }
    ));
  }

  exchange->market_buy(amount);
  // if we don't have a callback unlock lock right away
  if (!market_callback)
    execution_lock.unlock();
}

void BitcoinTrader::market_sell(double amount, function<void(double, double, long)> callback) {
  execution_lock.lock();

  market_callback = callback;

  ostringstream os;
  os << "MARKET SELLING " << amount << " BTC @ " << tick.bid;
  trading_log->output(os.str());

  // none of this is required if we don't have a callback
  if (market_callback) {
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        if (order_id == "failed") {
          market_callback(0, 0, 0);
          market_callback = nullptr;
          execution_lock.unlock();
        }
        else
          exchange->orderinfo(order_id);
      }
    ));
    exchange->set_orderinfo_callback(function<void(OrderInfo)>(
      [&](OrderInfo orderinfo) {
        market_callback(orderinfo.avg_price, orderinfo.filled_amount, orderinfo.create_date);
        market_callback = nullptr;
        execution_lock.unlock();
      }
    ));
  }

  exchange->market_sell(amount);
  // if we don't have a callback unlock lock right away
  if (!market_callback)
    execution_lock.unlock();
}

void BitcoinTrader::GTC_buy(double amount, double price, function<void(string)> callback) {
  execution_lock.lock();

  GTC_callback = callback;

  ostringstream os;
  os << "LIMIT BUYING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  exchange->set_trade_callback(function<void(string)>(
    [&](string order_id) {
      trading_log->output("LIMIT ORDER_ID: " + order_id);
      if(GTC_callback) {
        GTC_callback(order_id);
        GTC_callback = nullptr;
      }
      execution_lock.unlock();
    }
  ));

  exchange->limit_buy(amount, price);
}

void BitcoinTrader::GTC_sell(double amount, double price, function<void(string)> callback) {
  execution_lock.lock();

  GTC_callback = callback;

  ostringstream os;
  os << "LIMIT SELLING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  exchange->set_trade_callback(function<void(string)>(
    [&](string order_id) {
      trading_log->output("LIMIT ORDER_ID: " + order_id);
      if(GTC_callback) {
        GTC_callback(order_id);
        GTC_callback = nullptr;
      }
      execution_lock.unlock();
    }
  ));

  exchange->limit_sell(amount, price);
}
