#include "../include/okcoin_spot.h"

using namespace std::chrono;
using std::bind;         using std::thread;
using std::stringstream; using std::shared_ptr;
using std::string;       using std::function;
using std::map;          using std::this_thread::sleep_for;
using std::stol;         using std::stod;
using std::endl;         using std::lock_guard;
using std::mutex;        using std::exception;
using std::to_string;    using std::ifstream;
using std::ostringstream;using std::make_shared;
using std::next;

OKCoinSpot::OKCoinSpot(std::string name, shared_ptr<Log> log, shared_ptr<Config> config) :
  OKCoin(name, Spot, log, config) { }

void OKCoinSpot::subscribe_to_ticker() {
  subscribe_to_channel("ok_sub_spotusd_btc_ticker");
}

void OKCoinSpot::subscribe_to_OHLC(minutes period) {
  subscribe_to_channel("ok_sub_spotusd_btc_kline_" + period_s(period));
}

void OKCoinSpot::market_buy(double usd_amount) {
  order("buy_market", dtos(usd_amount, 2));
}

void OKCoinSpot::market_sell(double btc_amount) {
  order("sell_market", dtos(btc_amount, 4));
}

void OKCoinSpot::limit_buy(double amount, double price) {
  order("buy", dtos(amount, 4), dtos(price, 2));
}

void OKCoinSpot::limit_sell(double amount, double price) {
  order("sell", dtos(amount, 4), dtos(price, 2));
}

void OKCoinSpot::order(string type, string amount, string price) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_spotusd_trade";

  json p;
  // okcoin, for buy market orders, specifies amount to buy in USD using price field
  // and, for sell market orders, specifies amount to sell in USD using amount
  if (type == "buy_market")
    p["price"] = amount;
  else
    p["amount"] = amount;
  p["api_key"] = api_key;
  p["symbol"] = "btc_usd";
  p["type"] = type;
  if (price != "")
    p["price"] = price;
  p["sign"] = sign(p);

  j["parameters"] = p;

  ws.send(j.dump());
}

OKCoinSpot::BorrowInfo OKCoinSpot::borrow(Currency currency, double amount) {
  double rate = optionally_to_double(lend_depth(currency)[0]["rate"]);
  double can_borrow = optionally_to_double(borrows_info(currency)["can_borrow"]);

  if (amount > can_borrow)
    amount = can_borrow;

  BorrowInfo result;
  result.rate = rate;
  switch (currency) {
    case BTC : result.amount = truncate_to(amount, 2); break;
    case USD : result.amount = floor(amount); break;
  }

  auto response = borrow_money(currency, amount, rate);

  if (response["result"].get<bool>() == true)
    result.id = to_string(response["borrow_id"].get<long>());
  else
    result.id = "failed";

  return result;
}

double OKCoinSpot::close_borrow(Currency currency) {
  // get the open borrows
  auto j = unrepayments_info(currency);
  if (j.size() != 0) {
    string borrow_id = opt_to_string<int>(j[0]["borrow_id"]);

    // repay loan
    auto r = repayment(borrow_id);
    bool repaid = r["result"];

    if (repaid) {
      double amount = j[0]["amount"];
      return amount;
    }
    else
      return -1;
  }
  else
    return 0;
}

json OKCoinSpot::lend_depth(Currency currency) {
  string url = "https://www.okcoin.com/api/v1/lend_depth.do";

  json p;
  p["api_key"] = api_key;
  switch (currency) {
    case BTC : p["symbol"] = "btc_usd"; break;
    case USD : p["symbol"] = "usd"; break;
  }
  p["sign"] = sign(p);

  string response = curl_post(url, ampersand_list(p));
  auto j = json::parse(response);
  return j["lend_depth"];
}

json OKCoinSpot::borrows_info(Currency currency) {
  string url = "https://www.okcoin.com/api/v1/borrows_info.do";

  json p;
  p["api_key"] = api_key;
  switch (currency) {
    case BTC : p["symbol"] = "btc_usd"; break;
    case USD : p["symbol"] = "usd"; break;
  }
  p["sign"] = sign(p);

  string response = curl_post(url, ampersand_list(p));
  auto j = json::parse(response);
  return j;
}

json OKCoinSpot::unrepayments_info(Currency currency) {
  string url = "https://www.okcoin.com/api/v1/unrepayments_info.do";

  json p;
  p["api_key"] = api_key;
  switch (currency) {
    case BTC : p["symbol"] = "btc_usd"; break;
    case USD : p["symbol"] = "usd"; break;
  }
  p["current_page"] = 1;
  p["page_length"] = 10;
  p["sign"] = sign(p);

  string response = curl_post(url, ampersand_list(p));
  auto j = json::parse(response);
  return j["unrepayments"];
}

json OKCoinSpot::borrow_money(Currency currency, double amount, double rate) {
  string url = "https://www.okcoin.com/api/v1/borrow_money.do";

  json p;
  p["api_key"] = api_key;
  switch (currency) {
    case BTC : p["symbol"] = "btc_usd"; break;
    case USD : p["symbol"] = "usd"; break;
  }
  p["amount"] = amount;
  p["rate"] = rate;
  p["days"] = "fifteen";
  p["sign"] = sign(p);

  string response = curl_post(url, ampersand_list(p));
  auto j = json::parse(response);
  return j;
}

json OKCoinSpot::repayment(string borrow_id) {
  string url = "https://www.okcoin.com/api/v1/repayment.do";

  json p;
  p["api_key"] = api_key;
  p["borrow_id"] = borrow_id;
  p["sign"] = sign(p);

  string response = curl_post(url, ampersand_list(p));
  auto j = json::parse(response);
  return j;
}

void OKCoinSpot::orderinfo_handler(json order) {
  static map<int, string> statuses = {{-1, "cancelled"},
    {0, "unfilled"}, {1, "partially filled"},
    {2, "fully filled"}, {4, "cancel request in process"}};

  if (orderinfo_callback) {
    OrderInfo new_order;
    new_order.amount = optionally_to_double(order["amount"]);
    new_order.avg_price = optionally_to_double(order["avg_price"]);
    new_order.create_date = opt_to_string<long>(order["create_date"]);
    new_order.filled_amount = optionally_to_double(order["deal_amount"]);
    new_order.order_id = opt_to_string<long>(order["order_id"]);
    new_order.price = optionally_to_double(order["price"]);
    new_order.status = statuses[optionally_to_int(order["status"])];
    new_order.symbol = order["symbol"];
    new_order.type = order["type"];

    orderinfo_callback(order);
  }
}
