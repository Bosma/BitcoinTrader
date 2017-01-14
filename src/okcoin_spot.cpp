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

OKCoinSpot::OKCoinSpot(shared_ptr<Log> log, shared_ptr<Config> config) :
  OKCoin("OKCoinSpot", log, config) { }

void OKCoinSpot::on_message(string const & message) {
  ts_since_last = timestamp_now();

  try {
    auto j = json::parse(message);

    // if our json is an array
    // it's a response from some channel
    if (j.is_array()) {
      // there should be a channel key
      if (j[0].count("channel") == 1) {
        // fetch the channel name
        string channel = j[0]["channel"];
        // if we have a channel stored with that name
        // it's a price or data channel
        if (channels.count(channel) == 1) {
          // we have a channel in our map
          if (channels[channel]->status == "subscribing") {
            // if it's subscribing it's the first message we've received
            if (j[0].count("success") == 1) {
              if (j[0]["success"] == "true") {
                channels[channel]->status = "subscribed";
                log->output("SUBSCRIBED TO " + channel);
              }
              else {
                channels[channel]->status = "failed";
                log->output("UNSUCCESSFULLY SUBSCRIBED TO " + channel);
              }
            }
          }
          else if (channels[channel]->status == "subscribed") {
            // we're subscribed so delegate to a channel handler
            if (channel == "ok_sub_spotusd_btc_ticker") {
              // "buy":670.04,"high":684.91,"last":"670.14","low":659.38
              // "sell":670.44,"timestamp":"1467738929986","vol":"6,238.16"
              ticker_handler(j[0]["data"]);
            }
            else if (channel.find("ok_sub_spotusd_btc_kline_") != string::npos) {
              // remove first 25 letters (corresponding to length of above string)
              string period = channel.substr(25);
              auto data = j[0]["data"];
              if (data[0].is_array()) // data is an array of trades
                for (auto trade : data)
                  OHLC_handler(period, trade);
              else // data is a trade
                OHLC_handler(period, data);
            }
          }
          // store the last message received
          channels[channel]->last_message = message;
        }
        // we don't have a channel stored in the map
        // check for one off channel messages
        else if (channel == "ok_spotusd_trade") {
          // results of a trade
          if (j[0].count("errorcode") == 1) {
            string ec = j[0]["errorcode"];
            log->output("FAILED TRADE ON " + channel + " WITH ERROR: " + error_reasons[ec]);
            if (trade_callback)
              trade_callback("failed");
          }
          else {
            auto data = j[0]["data"];
            string order_id = data["order_id"];
            string result = data["result"];
            if (result == "true") {
              //log->output("TRADED (" + order_id + ") ON " + channel);
              if (trade_callback)
                trade_callback(order_id);
            }
            else {
              log->output("FAILED TRADE (" + order_id + ") ON " + channel + "WITH ERROR " + result);
              if (trade_callback)
                trade_callback("failed");
            }
          }
        }
        else if (channel == "ok_spotusd_orderinfo") {
          auto orders = j[0]["data"]["orders"];
          if (orders.empty())
            log->output(channel + " MESSAGE RECEIVED BY INVALID ORDER ID");
          else
            orderinfo_handler(orders[0]);

        }
        else if (channel == "ok_spotusd_userinfo") {
          if (j[0].count("errorcode") == 1) {
            string ec = j[0]["errorcode"];
            log->output("COULDN'T FETCH USER INFO WITH ERROR: " + error_reasons[ec]);
          }
          else {
            if (userinfo_callback) {
              auto funds = j[0]["data"]["info"]["funds"];
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
        }
        else {
          log->output("MESSAGE WITH UNKNOWN CHANNEL");
          log->output("RAW JSON: " + message);
        }
      }
      else {
        log->output("MESSAGE WITH NO HANDLER");
        log->output("RAW JSON: " + message);
      }
    }
    // message is not a channel message
    // so it's an event message
    else {
      if (j.count("event") == 1) {
        if (j["event"] == "pong") {
          if (pong_callback)
            pong_callback();
        }
        else {
          log->output("NOT ARRAY AND UNKNOWN EVENT: " + message);
        }
      }
      else {
        log->output("NOT ARRAY AND NO EVENT: " + message);
      }
    }
  }
  catch (exception& e) {
    stringstream ss;
    ss << "EXCEPTION IN on_message: " << e.what();
    log->output(ss.str() + " WITH JSON: " + message);
  }
}

void OKCoinSpot::subscribe_to_ticker() {
  subscribe_to_channel("ok_sub_spotusd_btc_ticker");
}

void OKCoinSpot::subscribe_to_OHLC(minutes period) {
  subscribe_to_channel("ok_sub_spotusd_btc_kline_" + period_conversions(period));
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

void OKCoinSpot::cancel_order(std::string order_id) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_spotusd_cancel_order";

  json p;
  p["api_key"] = api_key;
  p["symbol"] = "btc_usd";
  p["order_id"] = order_id;
  p["sign"] = sign("api_key=" + api_key + "&order_id=" + order_id + "&symbol=btc_usd&secret_key=" + secret_key);

  j["parameters"] = p;

  ws.send(j.dump());
}

void OKCoinSpot::order(string type, string amount, string price) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_spotusd_trade";

  json parameters;
  // okcoin, for buy market orders, specifies amount to buy in USD using price field
  // and, for sell market orders, specifies amount to sell in USD using amount
  if (type == "buy_market")
    parameters["price"] = amount;
  else
    parameters["amount"] = amount;
  parameters["api_key"] = api_key;
  parameters["symbol"] = "btc_usd";
  parameters["type"] = type;
  if (price != "")
    parameters["price"] = price;

  // signing parameters
  string str = "";
  // buy market doesn't use amount field
  if (type != "buy_market")
    str += "amount=" + amount + "&";
  str += "api_key=" + api_key;
  if (price != "")
    str += "&price=" + price;
  if (type == "buy_market")
    str += "&price=" + amount;
  str += "&symbol=btc_usd&type=" + type + "&secret_key=" + secret_key;

  parameters["sign"] = sign(str);

  j["parameters"] = parameters;

  ws.send(j.dump());
}

void OKCoinSpot::orderinfo(string order_id) {
  if (order_id == "failed") {
    OrderInfo failed_order;
    orderinfo_callback(failed_order);
  }
  else {
    json j;
    j["event"] = "addChannel";
    j["channel"] = "ok_spotusd_orderinfo";

    json parameters;
    parameters["api_key"] = api_key;
    parameters["symbol"] = "btc_usd";
    parameters["order_id"] = order_id;
    parameters["sign"] = sign("api_key=" + api_key + "&order_id=" + order_id + "&symbol=btc_usd&secret_key=" + secret_key);

    j["parameters"] = parameters;

    ws.send(j.dump());
  }
}

void OKCoinSpot::userinfo() {
  json j;
  j["event"] = "addChannel";
  j["channel"] = "ok_spotusd_userinfo";

  json p;
  p["api_key"] = api_key;
  p["sign"] = sign("api_key=" + api_key + "&secret_key=" + secret_key);

  j["parameters"] = p;
  ws.send(j.dump());
}


BorrowInfo OKCoinSpot::borrow(Currency currency, double amount) {
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

  auto response = borrow_money(currency, amount, rate, 15);

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

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_usd"; break;
    case USD : post_fields << "&symbol=usd"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j["lend_depth"];
}

json OKCoinSpot::borrows_info(Currency currency) {
  string url = "https://www.okcoin.com/api/v1/borrows_info.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_usd"; break;
    case USD : post_fields << "&symbol=usd"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j;
}

json OKCoinSpot::unrepayments_info(Currency currency) {
  string url = "https://www.okcoin.com/api/v1/unrepayments_info.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  post_fields << "&current_page=1";
  post_fields << "&page_length=10";
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_usd"; break;
    case USD : post_fields << "&symbol=usd"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j["unrepayments"];
}

json OKCoinSpot::borrow_money(Currency currency, double amount, double rate, int days) {
  string url = "https://www.okcoin.com/api/v1/borrow_money.do";

  ostringstream post_fields;
  post_fields << "amount=" << amount;
  post_fields << "&api_key=" << api_key;
  if (days == 15)
    post_fields << "&days=fifteen";
  post_fields << "&rate=" << rate;
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_usd"; break;
    case USD : post_fields << "&symbol=usd"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j;
}

json OKCoinSpot::repayment(string borrow_id) {
  string url = "https://www.okcoin.com/api/v1/repayment.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  post_fields << "&borrow_id=" << borrow_id;
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j;
}

void OKCoinSpot::OHLC_handler(string period, json trade) {
  if (OHLC_callback) {
    long timestamp = optionally_to_long(trade[0]);
    double open = optionally_to_double(trade[1]);
    double high = optionally_to_double(trade[2]);
    double low = optionally_to_double(trade[3]);
    double close = optionally_to_double(trade[4]);
    double volume = optionally_to_double(trade[5]);
    OHLC new_bar(timestamp, open, high, low, close, volume);
    OHLC_callback(period_conversions(period), new_bar);
  }
}

void OKCoinSpot::ticker_handler(json tick) {
  if (ticker_callback) {
    long timestamp = optionally_to_long(tick["timestamp"]);
    double last = optionally_to_double(tick["last"]);
    double bid = optionally_to_double(tick["buy"]);
    double ask = optionally_to_double(tick["sell"]);
    Ticker tick(timestamp, last, bid, ask);

    ticker_callback(tick);
  }
}

void OKCoinSpot::orderinfo_handler(json order) {
  static map<int, string> statuses = {{-1, "cancelled"},
    {0, "unfilled"}, {1, "partially filled"},
    {2, "fully filled"}, {4, "cancel request in process"}};

  if (orderinfo_callback) {
    double amount = optionally_to_double(order["amount"]);
    double avg_price = optionally_to_double(order["avg_price"]);
    string create_date = opt_to_string<long>(order["create_date"]);
    double filled_amount = optionally_to_double(order["deal_amount"]);
    string order_id = opt_to_string<long>(order["order_id"]);
    double price = optionally_to_double(order["price"]);
    string status = statuses[optionally_to_int(order["status"])];
    string symbol = order["symbol"];
    string type = order["type"];

    OrderInfo order(amount, avg_price, create_date,
        filled_amount, order_id, price, status, symbol, type);

    orderinfo_callback(order);
  }
}

void OKCoinSpot::backfill_OHLC(minutes period, int n) {
  ostringstream url;
  url << "https://www.okcoin.com/api/v1/kline.do?";
  url << "symbol=btc_usd";
  url << "&type=" << period_conversions(period);
  url << "&size=" << n;

  auto j = json::parse(curl_post(url.str()));

  for (auto each : j)
    OHLC_handler(period_conversions(period), each);
}
