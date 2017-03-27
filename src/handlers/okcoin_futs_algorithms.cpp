#include "handlers/okcoin_futs_handler.h"

using std::shared_ptr;
using std::to_string;
using std::string;
using std::chrono::seconds;
using namespace std::chrono_literals;
using std::this_thread::sleep_for;
using boost::optional;
using std::ostringstream;
using std::atomic;

bool OKCoinFutsHandler::limit(OKCoinFuts::OrderType type, double amount, int lever_rate, double limit_price,
                              std::chrono::seconds timeout) {
  // execution logging variables
  auto d1 = depth.get();
  auto t1 = timestamp_now();

  atomic<bool> trading_done(false);
  auto cancel_time = t1 + timeout;
  auto trade_callback = [&](const string& order_id) {
    if (order_id != "failed") {
      atomic<bool> done_limit_check(false);
      // until we are done,
      // we fully filled for the entire amount
      // or some seconds have passed
      OKCoinFuts::OrderInfo final_info{};

      okcoin_futs->set_orderinfo_callback(
          [&](const OKCoinFuts::OrderInfo& orderinfo) {
            if (orderinfo.status != OKCoin::OrderStatus::Failed) {
              final_info = orderinfo;
              // early stopping if we fill for the entire amount
              if (orderinfo.status == OKCoin::OrderStatus::FullyFilled)
                done_limit_check = true;
            }
            else // early stop if order status is failed (this shouldn't be called)
              done_limit_check = true;
          }
      );
      while (!done_limit_check &&
             timestamp_now() < cancel_time) {
        // fetch the orderinfo every second
        okcoin_futs->orderinfo(order_id, cancel_time);
        sleep_for(seconds(1));
      }

      if (final_info.status != OKCoin::OrderStatus::Failed) {
        // given the limit enough time, cancel it
        if (final_info.status != OKCoin::OrderStatus::FullyFilled)
          okcoin_futs->cancel_order(order_id, cancel_time);

        // execution logging
        auto d2 = depth.get();
        execution_logs.at("limit").row({
                                           to_string(static_cast<int>(type)),  // contract type
                                           to_string(t1.count()),              // start time
                                           to_string(final_info.timestamp.count()),              // finished time
                                           to_string(amount),                  // amount of contracts
                                           to_string(limit_price),             // limit price
                                           to_string(final_info.filled_amount),// amount filled
                                           to_string(final_info.avg_price),    // average price
                                           d1.to_string(),                     // depth at posting time
                                           d2.to_string()                      // depth after expiring
                                       });
      }
    }
    trading_done = true;
  };
  okcoin_futs->set_trade_callback(trade_callback);

  okcoin_futs->order(type, amount, limit_price, lever_rate, false, cancel_time);

  // check until the trade callback is finished, or cancel_time
  return check_until([&]() -> bool { return trading_done; }, cancel_time);
}

optional<OKCoinFuts::UserInfo> OKCoinFutsHandler::get_userinfo() {
  // Fetch the current OKCoin Futs account information (to get the equity)
  auto cancel_time = timestamp_now() + 10s;
  Atomic<OKCoinFuts::UserInfo> userinfo_a;
  okcoin_futs->set_userinfo_callback([&userinfo_a](const OKCoinFuts::UserInfo& new_userinfo) {
    userinfo_a.set(new_userinfo);
  });
  okcoin_futs->userinfo(cancel_time);
  if (check_until([&userinfo_a]() { return userinfo_a.has_been_set(); }, cancel_time)) {
    return userinfo_a.get();
  }
  else
    return {};
}