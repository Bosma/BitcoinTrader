#include "../include/exchange_handler.h"

using std::string;                 using std::function;
using std::make_shared;            using std::thread;
using std::this_thread::sleep_for; using std::chrono::seconds;
using std::cout;                   using std::endl;
using std::to_string;              using std::ostringstream;
using std::chrono::milliseconds;

bool check_until(function<bool()> test, seconds test_time, milliseconds time_between_checks) {
  auto t1 = timestamp_now();
  bool complete = false;
  bool completed_on_time = true;
  do {
    if (timestamp_now() - t1 > test_time)
      completed_on_time = false;
    else {
      if (test())
        complete = true;
      else
        sleep_for(time_between_checks);
    }
  } while (!complete);

  return completed_on_time;
}

Exchange::BorrowInfo BitcoinTrader::borrow(Currency currency, double amount) {
  Exchange::BorrowInfo result;

  if ((currency == BTC &&
       amount >= 0.01) ||
      (currency == CNY &&
       amount > 0.01 * tick.ask)) {

    auto successful_borrow = [&]() -> bool {
      result = exchange->borrow(currency, amount);
      return result.id != "failed";
    };
    if (check_until(successful_borrow, seconds(5), milliseconds(500))) {
      string cur = (currency == BTC) ? "BTC" : "CNY";
      trading_log->output("BORROWED " + to_string(result.amount) + " " + cur + " @ %" + to_string(result.rate) + " (" + result.id + ")");
    }
    else
      trading_log->output("TRIED TO BORROW FOR 5 SECONDS, GAVE UP");

  }

  return result;
}

void BitcoinTrader::close_short_then_long(double leverage) {
  trading_log->output("CLOSING MARGIN SHORT");

  Exchange::UserInfo info = get_userinfo();

  auto new_market_callback = [&, leverage](double amount, double price, string date) {
    if (date != "") {
      auto successfully_closed_borrow = [&, leverage]() -> bool {
        double result = exchange->close_borrow(Currency::BTC);

        if (result == 0)
          trading_log->output("REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
        else if (result == -1)
          trading_log->output("REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
        else
          trading_log->output("CLOSED " + to_string(result) + " BTC BORROW");
        
        return (result > 0);
      };
      if (check_until(successfully_closed_borrow, seconds(5), milliseconds(500)))
        margin_long(leverage);
      else
        trading_log->output("TRIED TO CLOSE BORROW FOR 5 SECONDS, GIVING UP");
    }
    else
      trading_log->output("FAILED TO BUY BTC, CANNOT PAY BACK");
  };
  set_market_callback(new_market_callback);

  market_buy(floor(info.free_cny));
}

void BitcoinTrader::close_long_then_short(double leverage) {
  trading_log->output("CLOSING MARGIN LONG");

  Exchange::UserInfo info = get_userinfo();

  auto new_market_callback = [&, leverage](double amount, double price, string date) {
    if (date != "") {
      auto successfully_closed_borrow = [&, leverage]() -> bool {
        double result = exchange->close_borrow(Currency::CNY);

        if (result == 0)
          trading_log->output("REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
        else if (result == -1)
          trading_log->output("REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
        else
          trading_log->output("CLOSED " + to_string(result) + " CNY BORROW");
        
        // we do not consider having nothing borrowed failure
        return (result >= 0);
      };
      if (check_until(successfully_closed_borrow, seconds(5), milliseconds(500)))
        margin_short(leverage);
      else
        trading_log->output("TRIED TO CLOSE BORROW FOR 5 SECONDS, GIVING UP");
    }
    else
      trading_log->output("FAILED TO SELL BTC, CANNOT PAY BACK");
  };
  set_market_callback(new_market_callback);

  market_sell(info.free_btc);
}

// this assumes no position already (nothing borrowed)
void BitcoinTrader::margin_long(double leverage) {

  trading_log->output("MARGIN LONGING " + to_string(leverage * 100) + "% of equity");

  auto no_open_position = [&]() -> bool {
    Exchange::UserInfo info = get_userinfo();
    return (info.borrow_btc == 0 && info.borrow_cny == 0);
  };
  if (check_until(no_open_position, seconds(5), milliseconds(50))) {
    Exchange::UserInfo info = get_userinfo();
    // grab price
    double price = tick.ask;
    // We have info.asset_net CNY of assets, so must own (equity_multiple * info.asset_net) / price of BTC
    double btc_to_own = (leverage * info.asset_net) / price;
    // We own info.free_btc of BTC, so need to buy btc_to_own - info.free_btc of BTC
    double btc_to_buy = btc_to_own - info.free_btc;
    // This will cost btc_to_buy * price worth of CNY to buy
    double cny_to_buy_btc = btc_to_buy * price;
    // We own info.free_cny of btc, so need to borrow cny_to_buy_btc - info.free_cny worth of CNY
    double cny_to_borrow = cny_to_buy_btc - info.free_cny;
    // We own info.free_cny of btc, so need to borrow cny_to_buy_btc - info.free_cny worth of CNY
    // borrow the CNY and go all BTC
    Exchange::BorrowInfo result = borrow(Currency::CNY, cny_to_borrow);

    if (result.amount > 0) {
      market_buy(floor(info.free_cny + result.amount));
    }
    // we failed to borrow, so just buy all CNY we own
    else
      market_buy(floor(info.free_cny));
  }
  else
    trading_log->output("ATTEMPTING TO MARGIN LONG WITH OPEN POSITION");

}

// this assumes no position already (nothing borrowed)
void BitcoinTrader::margin_short(double leverage) {

  trading_log->output("MARGIN SHORTING " + to_string(leverage * 100) + "% of equity");

  auto no_open_position = [&]() -> bool {
    Exchange::UserInfo info = get_userinfo();
    return (info.borrow_btc == 0 && info.borrow_cny == 0);
  };
  if (check_until(no_open_position, seconds(5), milliseconds(50))) {
    Exchange::UserInfo info = get_userinfo();
    // grab price
    double price = tick.bid;
    // We have info.asset_net of assets, so to be fully short must own info.asset_net * leverage of CNY
    double cny_to_own = info.asset_net * leverage;
    // We already own info.free_cny of CNY, so need to buy cny_to_own - info.free_cny of BTC
    double cny_to_buy = cny_to_own - info.free_cny;
    // This will mean we have to sell cny_to_buy / price BTC
    double btc_to_buy_cny = cny_to_buy / price;
    // We own info.free_btc of BTC already, so need to borrow btc_to_buy_cny - info.free_btc of BTC
    double btc_to_borrow = btc_to_buy_cny - info.free_btc;
    // borrow the BTC and sell it all
    Exchange::BorrowInfo result = borrow(Currency::BTC, btc_to_borrow);

    if (result.amount > 0) {
      market_sell(info.free_btc + result.amount);
    }
    // we failed to borrow, so just sell all BTC we own
    else
      market_sell(floor(info.free_btc));
  }
  else
    trading_log->output("ATTEMPTING TO MARGIN SHORT WITH OPEN POSITION");

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
      double filled_amount = 0;
      auto start_time = timestamp_now();
      bool done_limit_check = false;
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
        exchange->orderinfo(order_id);
        sleep_for(seconds(2));
      }

      // given the limit enough time, cancel it
      exchange->cancel_order(order_id);
      
      if (filled_amount != 0) {
        trading_log->output("limit_algorithm: FILLED FOR " + to_string(filled_amount) + " BTC");
        if (limit_callback)
          limit_callback(filled_amount);
      }
      else
        trading_log->output("limit_algorithm: NOT FILLED IN TIME");
    }
  ));
}

void BitcoinTrader::market_buy(double amount) {
  amount = truncate_to(amount, 2);

  if (amount > 0.01 * tick.ask) {
    // none of this is required if we don't have a callback
    if (market_callback) {
      // for 5 seconds, once a trade is confirmed, fetch its orderinfo
      auto new_trade_callback = [&](string order_id) {
        std::cout << "requesting orderinfo" << std::endl;
        auto t1 = timestamp_now();
        do {
          auto new_orderinfo_callback = [&](OrderInfo orderinfo) {
            std::cout << "received new orderinfo" << std::endl;
            set_current_order(orderinfo);
          };
          exchange->set_orderinfo_callback(new_orderinfo_callback);
          exchange->orderinfo(order_id);
          sleep_for(milliseconds(200));
        } while(timestamp_now() - t1 < seconds(5));
      };
      exchange->set_trade_callback(new_trade_callback);
    }

    trading_log->output("MARKET BUYING " + to_string(amount) + " CNY @ " + to_string(tick.ask));
    exchange->market_buy(amount);

    if (market_callback) {
      auto order_filled = [&]() -> bool {
        OrderInfo info = get_current_order();
        std::cout << "checking orderinfo: " << info.status << std::endl;
        return (info.status == "fully filled");
      };
      if (check_until(order_filled, seconds(5), milliseconds(50))) {
        OrderInfo info = get_current_order();
        market_callback(info.filled_amount, info.avg_price, info.create_date);
      }
      else {
        trading_log->output("MARKET BUY NOT FILLED AFTER 5 SECONDS");
        market_callback(0, 0, "");
      }
    }
  }
  else {
    if (market_callback)
      market_callback(0, 0, "");
  }
}

void BitcoinTrader::market_sell(double amount) {
  amount = truncate_to(amount, 2);

  if (amount > 0.01) {
    // none of this is required if we don't have a callback
    if (market_callback) {
      // for 5 seconds, once a trade is confirmed, fetch its orderinfo
      auto new_trade_callback = [&](string order_id) {
        auto t1 = timestamp_now();
        do {
          auto new_orderinfo_callback = [&](OrderInfo orderinfo) {
            set_current_order(orderinfo);
          };
          exchange->set_orderinfo_callback(new_orderinfo_callback);
          exchange->orderinfo(order_id);
          sleep_for(milliseconds(200));
        } while(timestamp_now() - t1 < seconds(5));
      };
      exchange->set_trade_callback(new_trade_callback);
    }

    trading_log->output("MARKET SELLING " + to_string(amount) + " BTC @ " + to_string(tick.bid));
    exchange->market_buy(amount);

    if (market_callback) {
      auto order_filled = [&]() -> bool {
        OrderInfo info = get_current_order();
        return (info.status == "fully filled");
      };
      if (check_until(order_filled, seconds(5), milliseconds(50))) {
        OrderInfo info = get_current_order();
        market_callback(info.filled_amount, info.avg_price, info.create_date);
      }
      else {
        trading_log->output("MARKET SELL NOT FILLED AFTER 5 SECONDS");
        market_callback(0, 0, "");
      }
    }
  }
  else {
    if (market_callback)
      market_callback(0, 0, "");
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
