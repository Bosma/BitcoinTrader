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
using std::to_string;

OKCoinSpot::OKCoinSpot(std::string name, shared_ptr<Log> log, shared_ptr<Config> config) :
    OKCoin(name, Spot, log, config) { }

void OKCoinSpot::subscribe_to_ticker() {
  subscribe_to_channel("ok_sub_spotusd_btc_ticker");
}

void OKCoinSpot::subscribe_to_OHLC(minutes period) {
  subscribe_to_channel("ok_sub_spotusd_btc_kline_" + period_s(period));
}

bool OKCoinSpot::subscribed_to_OHLC(minutes period) {
  string channel = "ok_sub_futureusd_btc_kline_" + period_s(period);

  return channels.count(channel) == 1 &&
         channels[channel]->status == "subscribed";
}

void OKCoinSpot::market_buy(double usd_amount, nanoseconds timeout_time) {
  order("buy_market", dtos(usd_amount, 2), timeout_time);
}

void OKCoinSpot::market_sell(double btc_amount, nanoseconds timeout_time) {
  order("sell_market", dtos(btc_amount, 4), timeout_time);
}

void OKCoinSpot::limit_buy(double amount, double price, nanoseconds timeout_time) {
  order("buy", dtos(amount, 4), timeout_time, dtos(price, 2));
}

void OKCoinSpot::limit_sell(double amount, double price, nanoseconds timeout_time) {
  order("sell", dtos(amount, 4), timeout_time, dtos(price, 2));
}

void OKCoinSpot::order(string type, string amount, nanoseconds timeout_time, string price) {
  string channel = "ok_spotusd_trade";

  channel_timeouts[channel] = timeout_time;

  json j;

  j["event"] = "addChannel";
  j["channel"] = channel;

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
  string sig = sign(p);
  p["sign"] = sig;

  j["parameters"] = p;

  ws.send(j.dump());
}

OKCoinSpot::BorrowInfo OKCoinSpot::borrow(Currency currency, double amount) {
  BorrowInfo result;

  auto ld = lend_depth(currency);
  auto bi = borrows_info(currency);
  if (ld.empty() || bi.empty()) {
    return result;
  }

  json response;
  try {
    double rate = optionally_to_double(ld["lend_depth"][0]["rate"]);
    double can_borrow = optionally_to_double(bi["can_borrow"]);

    if (amount > can_borrow)
      amount = can_borrow;

    result.rate = rate;
    switch (currency) {
      case BTC : result.amount = truncate_to(amount, 2); break;
      case USD : result.amount = floor(amount); break;
    }

    response = borrow_money(currency, amount, rate);

    if (!response.empty() &&
        response["result"].get<bool>()) {
      result.id = to_string(response["borrow_id"].get<long>());
      result.valid = true;
    }
  }
  catch (exception& e) {
    log->output("OKCoinSpot::borrow(): ERROR PARSING JSON " + string(e.what()) + ", with: lend_depth: " + ld.dump() + ", borrows_info: " + bi.dump() + ", borrow_money: " + response.dump());
  }

  return result;
}

double OKCoinSpot::close_borrow(Currency currency) {
  // get the open borrows
  auto j = unrepayments_info(currency);
  if (!j.empty()) {
    json r;
    try {
      string borrow_id = opt_to_string<int>(j["unrepayments"][0]["borrow_id"]);

      // repay loan
      r = repayment(borrow_id);
      if (r.empty()) {
        return 0;
      }
      else {
        bool repaid = r["result"];
        if (repaid) {
          double amount = j["unrepayments"][0]["amount"];
          return amount;
        }
        else
          return -1;
      }
    }
    catch (exception& e) {
      log->output("OKCoinSpot::close_borrow(): ERROR PARSING JSON " + string(e.what()) + ", with: unrepayments_info: " + j.dump() + ", repayment: " + r.dump());
    }
  }
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
  string sig = sign(p);
  p["sign"] = sig;

  string response = curl_post(url, log, ampersand_list(p));
  json j;
  if (!response.empty() && response.at(0) != '<') {
    try {
      j = json::parse(response);
    }
    catch (exception &e) {
      log->output("OKCoinSpot::lend_depth(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
    }
  }
  return j;
}

json OKCoinSpot::borrows_info(Currency currency) {
  string url = "https://www.okcoin.com/api/v1/borrows_info.do";

  json p;
  p["api_key"] = api_key;
  switch (currency) {
    case BTC : p["symbol"] = "btc_usd"; break;
    case USD : p["symbol"] = "usd"; break;
  }
  string sig = sign(p);
  p["sign"] = sig;

  string response = curl_post(url, log, ampersand_list(p));
  json j;
  if (!response.empty() && response.at(0) != '<') {
    try {
      j = json::parse(response);
    }
    catch (exception &e) {
      log->output("OKCoinSpot::borrows_info(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
    }
  }
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
  string sig = sign(p);
  p["sign"] = sig;

  string response = curl_post(url, log, ampersand_list(p));
  json j;
  if (!response.empty() && response.at(0) != '<') {
    try {
      j = json::parse(response);
    }
    catch (exception &e) {
      log->output("OKCoinSpot::unrepayments_info(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
    }
  }
  return j;
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
  string sig = sign(p);
  p["sign"] = sig;

  string response = curl_post(url, log, ampersand_list(p));
  json j;
  if (!response.empty() && response.at(0) != '<') {
    try {
      j = json::parse(response);
    }
    catch (exception &e) {
      log->output("OKCoinSpot::borrow_money(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
    }
  }
  return j;
}

json OKCoinSpot::repayment(string borrow_id) {
  string url = "https://www.okcoin.com/api/v1/repayment.do";

  json p;
  p["api_key"] = api_key;
  p["borrow_id"] = borrow_id;
  string sig = sign(p);
  p["sign"] = sig;

  string response = curl_post(url, log, ampersand_list(p));
  json j;
  if (!response.empty() && response.at(0) != '<') {
    try {
      j = json::parse(response);
    }
    catch (exception &e) {
      log->output("OKCoinSpot::repayment(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
    }
  }
  return j;
}

bool OKCoinSpot::backfill_OHLC(minutes period, unsigned long n) {
  ostringstream url;
  url << "https://www.okcoin.com/api/v1/kline.do?symbol=btc_usd";
  url << "&type=" << period_s(period);
  url << "&size=" << n;

  auto response = curl_post(url.str(), log);
  json j;
  if (!response.empty() && response.at(0) != '<') {
    try {
      j = json::parse(response);
      for (auto each : j)
        OHLC_handler(period_s(period), each);
    }
    catch (exception &e) {
      log->output("OKCoinSpot::backfill_OHLC(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
      return false;
    }
  }
  return true;
}

void OKCoinSpot::orderinfo_handler(json order) {

  if (orderinfo_callback) {
    OrderInfo new_order;
    new_order.amount = optionally_to_double(order["amount"]);
    new_order.avg_price = optionally_to_double(order["avg_price"]);
    new_order.create_date = opt_to_string<long>(order["create_date"]);
    new_order.filled_amount = optionally_to_double(order["deal_amount"]);
    new_order.order_id = opt_to_string<long>(order["order_id"]);
    new_order.price = optionally_to_double(order["price"]);
    new_order.status = static_cast<OrderStatus>(optionally_to_int(order["status"]));
    new_order.symbol = order["symbol"];
    new_order.type = order["type"];

    orderinfo_callback(new_order);
  }
}

void OKCoinSpot::userinfo_handler(json j) {
  if (userinfo_callback) {
    auto funds = j["info"]["funds"];
    UserInfo info;
    info.asset_net = optionally_to_double(funds["asset"]["net"]);
    info.free[Currency::BTC] = optionally_to_double(funds["free"]["btc"]);
    info.free[Currency::USD] = optionally_to_double(funds["free"]["usd"]);
    if (funds.count("borrow") == 1) {
      info.borrow[Currency::BTC] = optionally_to_double(funds["borrow"]["btc"]);
      info.borrow[Currency::USD] = optionally_to_double(funds["borrow"]["usd"]);
    }
    else {
      info.borrow[Currency::BTC] = 0;
      info.borrow[Currency::USD] = 0;
    }
    userinfo_callback(info);
  }
}
