#include "../include/exchange_handler.h"

using std::string;                 using std::function;
using std::make_shared;            using std::thread;
using std::this_thread::sleep_for; using std::chrono::seconds;
using std::cout;                   using std::endl;
using std::to_string;              using std::ostringstream;
using std::chrono::milliseconds;

bool check_until(function<bool()> test, seconds test_time, milliseconds time_between_checks = milliseconds(50)) {
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

BorrowInfo BitcoinTrader::borrow(Currency currency, double amount) {
  BorrowInfo result;

  if ((currency == BTC && amount >= 0.01) ||
      (currency == CNY && amount > 0.01 * tick.ask)) {

    auto successful_borrow = [&]() -> bool {
      result = exchange->borrow(currency, amount);
      return result.id != "failed";
    };
    if (check_until(successful_borrow, seconds(10), seconds(1))) {
      string cur = (currency == BTC) ? "BTC" : "CNY";
      trading_log->output("BORROWED " + to_string(result.amount) + " " + cur + " @ %" + to_string(result.rate) + " (" + result.id + ")");
    }
    else
      trading_log->output("TRIED TO BORROW FOR 5 SECONDS, GAVE UP");

  }

  return result;
}

void BitcoinTrader::close_position_then(Direction direction, double leverage) {
  Currency currency = dir_to_tx(direction);

  trading_log->output("CLOSING MARGIN " + dir_to_string(direction));

  UserInfo info = get_userinfo();

  if (info.borrow[currency] == 0) {
    margin(direction, leverage);
  }
  else {
    auto new_market_callback = [&, leverage, direction, currency](double amount, double price, string date) {
      if (date != "") {
        trading_log->output(dir_to_past_tense(direction) + " " + to_string(amount) + " " + cur_to_string(currency) + " @ " + to_string(price));

        auto successfully_closed_borrow = [&]() -> bool {
          double result = exchange->close_borrow(currency);

          if (result == 0)
            trading_log->output("REQUESTED CLOSE BORROW BUT NOTHING BORROWED");
          else if (result == -1)
            trading_log->output("REQUESTED CLOSE BORROW BUT FAILED TO REPAY");
          else
            trading_log->output("CLOSED " + to_string(result) + " " + cur_to_string(currency) + " BORROW");

          // we do not consider having nothing borrowed failure
          return (result >= 0);
        };
        if (check_until(successfully_closed_borrow, seconds(5), milliseconds(500))) {
          auto reflected_in_userinfo = [&]() -> bool {
            UserInfo info = get_userinfo();
            return (info.borrow[currency] == 0);
          };
          if (check_until(reflected_in_userinfo, seconds(5)))
            margin(direction, leverage);
          else
            trading_log->output("CLOSED BORROW BUT NOT REFLECTED IN USERINFO");
        }
        else
          trading_log->output("TRIED TO CLOSE BORROW FOR 5 SECONDS, GIVING UP");
      }
      else
        trading_log->output("FAILED TO " + dir_to_action(direction) + " " + cur_to_string(currency) + ", CANNOT PAY BACK");
    };
    set_market_callback(new_market_callback);

    market(direction, info.free[currency]);
  }
}

void BitcoinTrader::margin(Direction direction, double leverage) {
  trading_log->output("MARGIN " + dir_to_string(direction) + "ING " + to_string(leverage * 100) + "% of equity");

  UserInfo info = get_userinfo();

  double want_to_own = leverage * info.asset_net;
  double already_own = info.free[dir_to_own(direction)];
  // if direction units are in BTC, we need to convert to CNY to compare with net assets
  if (direction == Direction::Long)
    already_own *= dir_to_price(direction);

  if (already_own < want_to_own) {
    double amount_to_transact = want_to_own - already_own;
    // if we're going short, convert the units from CNY to BTC
    if (direction == Direction::Short)
      amount_to_transact /= dir_to_price(direction);

    market_callback = nullptr;
    // check if we have to borrow any
    if (amount_to_transact > info.free[dir_to_tx(direction)]) {
      // we have to borrow, calculate how much to borrow
      double amount_to_borrow = amount_to_transact - info.free[dir_to_tx(direction)];
      BorrowInfo result = borrow(dir_to_tx(direction), amount_to_borrow);
      if (result.amount > 0) {
        // we've borrowed, but wait until we can spend the money
        auto borrow_spendable = [&]() -> bool {
          UserInfo info = get_userinfo();
          return (info.free[dir_to_tx(direction)] >= result.amount);
        };
        if (check_until(borrow_spendable, seconds(10)))
          market(direction, info.free[dir_to_tx(direction)] + result.amount);
        else {
          trading_log->output("BORROW SUCCEEDED BUT NOT SEEING IT IN BALANCE");
          market(direction, amount_to_transact);
        }
      }
      else {
        market(direction, amount_to_transact);
      }
    }
    else {
      trading_log->output("DO NOT HAVE TO BORROW ANYTHING");
      market(direction, amount_to_transact);
    }
  }
}

void BitcoinTrader::limit(Direction direction, double amount, double price, seconds cancel_time) {
  exchange->set_trade_callback(function<void(string)>(
    [&](string order_id) {
      double filled_amount = 0;
      auto start_time = timestamp_now();
      bool done_limit_check = false;
      // until we are done,
      // we fully filled for the entire amount
      // or some seconds have passed
      while (!done && !done_limit_check &&
          timestamp_now() - start_time < cancel_time) {
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
        trading_log->output("FILLED FOR " + to_string(filled_amount) + " BTC");
        if (limit_callback)
          limit_callback(filled_amount);
      }
      else
        trading_log->output("NOT FILLED IN TIME");
    }
  ));

  trading_log->output("LIMIT " + dir_to_action(direction) + "ING " + to_string(amount) + " BTC @ " + to_string(price));

  if (direction == Direction::Long)
    exchange->limit_buy(amount, price);
  else
    exchange->limit_sell(amount, price);
}

void BitcoinTrader::market(Direction direction, double amount) {
  if (direction == Direction::Long)
    amount = floor(amount);
  else
    amount = truncate_to(amount, 2);

  if ((direction == Direction::Long && amount > 0.01 * tick.ask) ||
      (direction == Direction::Short && amount > 0.01)) {
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
          sleep_for(milliseconds(1000));
        } while(timestamp_now() - t1 < seconds(5));
      };
      exchange->set_trade_callback(new_trade_callback);
    }

    if (direction == Direction::Long) {
      double estimated_btc = amount / dir_to_price(direction);
      trading_log->output("MARKET BUYING USING " + to_string(amount) + " CNY (" + to_string(estimated_btc) + " BTC) @ " + to_string(dir_to_price(direction)));
      exchange->market_buy(amount);
    }
    else {
      double estimated_cny = amount * dir_to_price(direction);
      trading_log->output("MARKET SELLING " + to_string(amount) + " BTC (" + to_string(estimated_cny) + " CNY) @ " + to_string(dir_to_price(direction)));
      exchange->market_sell(amount);
    }

    if (market_callback) {
      auto order_filled = [&]() -> bool {
        OrderInfo info = get_current_order();
        trading_log->output("FETCHED ORDERINFO: " + info.to_string());
        return (info.status == "fully filled");
      };
      clear_current_order();
      if (check_until(order_filled, seconds(5))) {
        OrderInfo info = get_current_order();
        market_callback(info.filled_amount, info.avg_price, info.create_date);
      }
      else {
        trading_log->output("MARKET " + dir_to_action(direction) + " NOT FILLED AFTER 5 SECONDS");
        market_callback(0, 0, "");
      }
    }
  }
  else {
    if (market_callback)
      market_callback(0, 0, "");
  }
}

void BitcoinTrader::GTC(Direction direction, double amount, double price) {
  if (GTC_callback) {
    exchange->set_trade_callback(function<void(string)>(
      [&](string order_id) {
        if(GTC_callback)
          GTC_callback(order_id);
      }
    ));
  }

  trading_log->output("LIMIT " + dir_to_action(direction) + "ING " + to_string(amount) + " BTC @ " + to_string(price));

  if (direction == Direction::Long)
    exchange->limit_buy(amount, price);
  else
    exchange->limit_sell(amount, price);
}
