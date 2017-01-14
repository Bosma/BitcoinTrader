#include "../include/exchange_handler.h"

using std::string;                 using std::function;
using std::make_shared;            using std::thread;
using std::this_thread::sleep_for; using std::chrono::seconds;
using std::cout;                   using std::endl;
using std::to_string;              using std::ostringstream;
using std::chrono::milliseconds;

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
      trading_log->output("MARKET BUYING USING " + to_string(amount) + " USD (" + to_string(estimated_btc) + " BTC) @ " + to_string(dir_to_price(direction)));
      exchange->market_buy(amount);
    }
    else {
      double estimated_usd = amount * dir_to_price(direction);
      trading_log->output("MARKET SELLING " + to_string(amount) + " BTC (" + to_string(estimated_usd) + " USD) @ " + to_string(dir_to_price(direction)));
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
