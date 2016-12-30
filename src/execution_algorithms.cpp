#include "../include/exchange_handler.h"

using std::string;                 using std::function;
using std::make_shared;            using std::thread;
using std::this_thread::sleep_for; using std::chrono::seconds;
using std::cout;                   using std::endl;
using std::to_string;              using std::ostringstream;
using std::chrono::milliseconds;

double BitcoinTrader::borrow(Currency currency, double amount) {
  if ((currency == BTC &&
       amount >= 0.01) ||
      (currency == CNY &&
       amount > 0.01 * tick.ask)) {
    Exchange::BorrowInfo result = exchange->borrow(currency, amount);
    string cur = (currency == BTC) ? "BTC" : "CNY";
    if (result.id == "failed")
      trading_log->output("FAILED TO BORROW " + to_string(result.amount) + " " + cur + " @ %" + to_string(result.rate));
    else {
      trading_log->output("BORROWED " + to_string(result.amount) + " " + cur + " @ %" + to_string(result.rate) + " (" + result.id + ")");
      return result.amount;
    }
  }
  return 0;
}

// this assumes no position already (nothing borrowed)
void BitcoinTrader::margin_long(double equity_multiple) {
  exchange->set_userinfo_callback([&, equity_multiple](Exchange::UserInfo info) {
    if (info.borrow_btc == 0 && info.borrow_cny == 0) {
      // grab price
      double price = tick.ask;
      // We have info.asset_net CNY of assets, so must own (equity_multiple * info.asset_net) / price of BTC
      double btc_to_own = (equity_multiple * info.asset_net) / price;
      // We own info.free_btc of BTC, so need to buy btc_to_own - info.free_btc of BTC
      double btc_to_buy = btc_to_own - info.free_btc;
      // This will cost btc_to_buy * price worth of CNY to buy
      double cny_to_buy_btc = btc_to_buy * price;
      // We own info.free_cny of btc, so need to borrow cny_to_buy_btc - info.free_cny worth of CNY
      double cny_to_borrow = cny_to_buy_btc - info.free_cny;
      // We own info.free_cny of btc, so need to borrow cny_to_buy_btc - info.free_cny worth of CNY
      // borrow the CNY and go all BTC
      double amount_borrowed = borrow(Currency::CNY, cny_to_borrow);
      if (amount_borrowed > 0) {
        sleep_for(seconds(1));
        market_buy(floor(info.free_cny + amount_borrowed));
      }
      // we failed to borrow, so just buy all CNY we own
      else
        market_buy(floor(info.free_cny));
    }
    else
      trading_log->output("ATTEMPTING TO MARGIN LONG WITH OPEN POSITION");
  });

  trading_log->output("MARGIN LONGING " + to_string(equity_multiple * 100) + "% of equity");

  // need to find out how much net assets we have
  exchange->userinfo();
}

// this assumes no position already (nothing borrowed)
void BitcoinTrader::margin_short(double equity_multiple) {
  exchange->set_userinfo_callback([&, equity_multiple](Exchange::UserInfo info) {
    if (info.borrow_btc == 0 && info.borrow_cny == 0) {
      // grab price
      double price = tick.bid;
      // We have info.asset_net of assets, so to be fully short must own info.asset_net * equity_multiple of CNY
      double cny_to_own = info.asset_net * equity_multiple;
      // We already own info.free_cny of CNY, so need to buy cny_to_own - info.free_cny of BTC
      double cny_to_buy = cny_to_own - info.free_cny;
      // This will mean we have to sell cny_to_buy / price BTC
      double btc_to_buy_cny = cny_to_buy / price;
      // We own info.free_btc of BTC already, so need to borrow btc_to_buy_cny - info.free_btc of BTC
      double btc_to_borrow = btc_to_buy_cny - info.free_btc;
      // borrow the BTC and sell it all
      double amount_borrowed = borrow(Currency::BTC, btc_to_borrow);
      if (amount_borrowed > 0) {
        sleep_for(seconds(1));
        market_sell(info.free_btc + amount_borrowed);
      }
      else
        market_sell(info.free_btc);
    }
    else
      trading_log->output("ATTEMPTING TO MARGIN SHORT WITH OPEN POSITION");
  });

  trading_log->output("MARGIN SHORTING " + to_string(equity_multiple * 100) + "% of equity");

  // need to find out how much net assets we have
  exchange->userinfo();
}

void BitcoinTrader::close_margin_short() {
  exchange->set_userinfo_callback([&](Exchange::UserInfo info) {
    market_buy(floor(info.free_cny));

    sleep_for(seconds(2));

    double result = exchange->close_borrow(Currency::BTC);
    if (result == 0)
      trading_log->output("REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
    else if (result == -1)
      trading_log->output("REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
    else
      trading_log->output("CLOSED " + to_string(result) + " BTC BORROW");
  });

  trading_log->output("CLOSING MARGIN SHORT");
  exchange->userinfo();
}

void BitcoinTrader::close_margin_long() {
  exchange->set_userinfo_callback([&](Exchange::UserInfo info) {
    market_sell(info.free_btc);

    sleep_for(seconds(2));

    double result = exchange->close_borrow(Currency::CNY);
    if (result == 0)
      trading_log->output("REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
    else if (result == -1)
      trading_log->output("REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
    else
      trading_log->output("CLOSED " + to_string(result) + " CNY BORROW");
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
  amount = truncate_to(amount, 2);
  if (amount > 0.01 * tick.ask) {
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
    os << "MARKET BUYING " << amount << " CNY @ " << tick.ask;
    trading_log->output(os.str());

    exchange->market_buy(amount);
  }
  else {
    if (market_callback)
      market_callback(0, 0, 0);
  }
}

void BitcoinTrader::market_sell(double amount) {
  amount = truncate_to(amount, 2);
  if (amount >= 0.01) {
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
    os << "MARKET SELLING " << amount << " BTC @ " << tick.bid;
    trading_log->output(os.str());

    exchange->market_sell(amount);
  }
  else {
    if (market_callback)
      market_callback(0, 0, 0);
  }
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
