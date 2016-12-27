#include "../include/exchange_handler.h"

using std::string;                 using std::function;
using std::make_shared;            using std::thread;
using std::this_thread::sleep_for; using std::chrono::seconds;
using std::cout;                   using std::endl;
using std::to_string;              using std::ostringstream;

void BitcoinTrader::full_margin_long(double percent) {
  exchange->set_userinfo_callback([&](double btc, double cny) {
    // after we know how much CNY we now have, market buy it all
    set_market_callback([&](double avg_price, double amount, long date) {
      if (date != 0)
        trading_log->output("full_margin_long: BOUGHT " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");
      else
        trading_log->output("full_margin_long: FAILED TO BUY");
    });
    market_buy(floor(cny));
  });

  trading_log->output("GOING FULL MARGIN LONG");
  // borrowing MAX CNY
  Exchange::BorrowInfo result = exchange->borrow(Currency::CNY, percent);
  // wait a bit after borrowing
  sleep_for(seconds(1));
  if (result.amount == 0)
    trading_log->output("full_margin_long: CANNOT BORROW ANY CNY");
  else if (result.id == "failed")
    trading_log->output("full_margin_long: FAILED TO BORROW " + to_string(result.amount) + " CNY @ %" + to_string(result.rate));
  else
    trading_log->output("full_margin_long: BORROWED " + to_string(result.amount) + " CNY @ %" + to_string(result.rate));

  // need to find out how much CNY we have now
  exchange->userinfo();
}

void BitcoinTrader::full_margin_short(double percent) {
  exchange->set_userinfo_callback([&](double btc, double cny) {
    // after we know how much BTC we now have, market sell it all
    set_market_callback([&](double avg_price, double amount, long date) {
      if (date != 0)
        trading_log->output("full_margin_short: SOLD " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");
      else
        trading_log->output("full_margin_short: FAILED TO SELL");
    });
    market_sell(btc);
  });
  
  // borrowing MAX BTC
  Exchange::BorrowInfo result = exchange->borrow(Currency::BTC, percent);
  // wait a bit after borrowing
  sleep_for(seconds(1));
  if (result.amount == 0)
    trading_log->output("full_margin_short: CANNOT BORROW ANY BTC");
  else if (result.id == "failed")
    trading_log->output("full_margin_short: FAILED TO BORROW " + to_string(result.amount) + " BTC @ %" + to_string(result.rate));
  else
    trading_log->output("full_margin_short: BORROWED " + to_string(result.amount) + " BTC @ %" + to_string(result.rate));

  // need to find out how much BTC we have now
  exchange->userinfo();
}

void BitcoinTrader::close_margin_short() {
  exchange->set_userinfo_callback([&](double btc, double cny) {
    set_market_callback([&](double avg_price, double amount, long date) {
      if (date != 0)
        trading_log->output("close_margin_short: BOUGHT " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");
      else
        trading_log->output("close_margin_short: FAILED TO BUY");

      double result = exchange->close_borrow(Currency::BTC);
      if (result == 0)
        trading_log->output("close_margin_short: REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
      else if (result == -1)
        trading_log->output("close_margin_short: REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
      else
        trading_log->output("close_margin_short: CLOSED " + to_string(result) + " BTC BORROW");
    });
    market_buy(floor(cny));
  });

  trading_log->output("CLOSING MARGIN SHORT");
  exchange->userinfo();
}

void BitcoinTrader::close_margin_long() {
  exchange->set_userinfo_callback([&](double btc, double cny) {
    set_market_callback([&](double avg_price, double amount, long date) {
      if (date != 0)
        trading_log->output("close_margin_long: SOLD " + to_string(amount) + " BTC @ " + to_string(avg_price) + " CNY");
      else
        trading_log->output("close_margin_long: FAILED TO SELL");

      double result = exchange->close_borrow(Currency::CNY);
      if (result == 0)
        trading_log->output("close_margin_long: REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
      else if (result == -1)
        trading_log->output("close_margin_long: REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
      else
        trading_log->output("close_margin_long: CLOSED " + to_string(result) + " CNY BORROW");
    });
    market_sell(btc);
  });

  trading_log->output("CLOSING MARGIN LONG");
  exchange->userinfo();
}

void BitcoinTrader::limit_buy(double amount, double price, seconds cancel_time) {
  limit_algorithm(cancel_time);

  ostringstream os;
  os << "limit_buy: LIMIT BUYING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  exchange->limit_buy(amount, price);
}

void BitcoinTrader::limit_sell(double amount, double price, seconds cancel_time) {
  limit_algorithm(cancel_time);

  ostringstream os;
  os << "limit_sell: LIMIT SELLING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  exchange->limit_sell(amount, price);
}

void BitcoinTrader::limit_algorithm(seconds limit) {
  exchange->set_trade_callback(function<void(string)>(
    [&](string order_id) {
      current_limit = order_id;
      running_threads["limit"] = make_shared<thread>([&]() {
        filled_amount = 0;
        auto start_time = timestamp_now();
        done_limit_check = false;
        // until we are done,
        // we fully filled for the entire amount
        // or some seconds have passed
        while (!done && !done_limit_check &&
            timestamp_now() - start_time < limit) {
          // fetch the orderinfo every 2 seconds
          exchange->set_orderinfo_callback(function<void(OrderInfo)>(
            [&](OrderInfo orderinfo) {
              // early stopping if we fill for the entire amount
              if (orderinfo.amount == orderinfo.filled_amount)
                done_limit_check = true;
              filled_amount = orderinfo.filled_amount;
            }
          ));
          exchange->orderinfo(current_limit);
          sleep_for(seconds(2));
        }

        // given the limit enough time, cancel it
        exchange->cancel_order(current_limit);
        
        if (filled_amount != 0) {
          trading_log->output("limit_algorithm: FILLED FOR " + to_string(filled_amount) + " BTC");
          if (limit_callback)
            limit_callback(filled_amount);
        }
        else
          trading_log->output("limit_algorithm: NOT FILLED IN TIME");
      });
    }
  ));
}

void BitcoinTrader::market_buy(double amount) {
  // none of this is required if we don't have a callback
  if (market_callback) {
    exchange->set_orderinfo_callback(function<void(OrderInfo)>(
      [&](OrderInfo orderinfo) {
        market_callback(orderinfo.avg_price, orderinfo.filled_amount, orderinfo.create_date);
      }
    ));
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        exchange->orderinfo(order_id);
      }
    ));
  }

  ostringstream os;
  os << "market_buy: MARKET BUYING " << amount << " CNY @ " << tick.ask;
  trading_log->output(os.str());

  exchange->market_buy(amount);
}

void BitcoinTrader::market_sell(double amount) {
  // none of this is required if we don't have a callback
  if (market_callback) {
    exchange->set_orderinfo_callback(function<void(OrderInfo)>(
      [&](OrderInfo orderinfo) {
        market_callback(orderinfo.avg_price, orderinfo.filled_amount, orderinfo.create_date);
      }
    ));
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        exchange->orderinfo(order_id);
      }
    ));
  }

  ostringstream os;
  os << "market_sell: MARKET SELLING " << amount << " BTC @ " << tick.bid;
  trading_log->output(os.str());

  exchange->market_sell(amount);
}

void BitcoinTrader::GTC_buy(double amount, double price) {
  if (GTC_callback) {
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        if(GTC_callback)
          GTC_callback(order_id);
      }
    ));
  }

  ostringstream os;
  os << "GTC_buy: LIMIT BUYING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  exchange->limit_buy(amount, price);
}

void BitcoinTrader::GTC_sell(double amount, double price) {
  if (GTC_callback) {
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        if(GTC_callback)
          GTC_callback(order_id);
      }
    ));
  }

  ostringstream os;
  os << "GTC_sell: LIMIT SELLING " << amount << " BTC @ " << price;
  trading_log->output(os.str());

  exchange->limit_sell(amount, price);
}
