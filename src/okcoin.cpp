#include "../include/okcoin.h"

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

OKCoin::OKCoin(shared_ptr<Log> log, shared_ptr<Config> config) :
  Exchange("OKCoin", log, config),
  api_key((*config)["okcoin_apikey"]),
  secret_key((*config)["okcoin_secretkey"]),
  ws(OKCOIN_URL),
  error_reasons()
{
  ws.set_open_callback( bind(&OKCoin::on_open, this) );
  ws.set_message_callback( bind(&OKCoin::on_message, this, std::placeholders::_1) );
  ws.set_close_callback( bind(&OKCoin::on_close, this) );
  ws.set_fail_callback( bind(&OKCoin::on_fail, this) );
  ws.set_error_callback( bind(&OKCoin::on_error, this, std::placeholders::_1) );

  populate_error_reasons();
}

OKCoin::~OKCoin() {
}

void OKCoin::start() {
  ws.connect();
}

void OKCoin::on_open() {
  log->output("OPENED SOCKET to " + ws.get_uri());
  
  // if we're open, no need to reconnect
  reconnect = false;

  if (open_callback)
    open_callback();
}

void OKCoin::on_close() {
  log->output("CLOSE with reason: " + ws.get_error_reason());

  // 1001 is normal close
  if (ws.get_close_code() != 1001) {
    log->output("WS ABNORMAL CLOSE CODE");
    reconnect = true;
  }
}

void OKCoin::on_message(string const & message) {
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
            if (channel == "ok_sub_spotcny_btc_ticker") {
              // "buy":670.04,"high":684.91,"last":"670.14","low":659.38
              // "sell":670.44,"timestamp":"1467738929986","vol":"6,238.16"
              ticker_handler(j[0]["data"]);
            }
            else if (channel.find("ok_sub_spotcny_btc_kline_") != string::npos) {
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
        else if (channel == "ok_spotcny_trade") {
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
        else if (channel == "ok_spotcny_orderinfo") {
          auto orders = j[0]["data"]["orders"];
          if (orders.empty())
            log->output(channel + " MESSAGE RECEIVED BY INVALID ORDER ID");
          else
            orderinfo_handler(orders[0]);

        }
        else if (channel == "ok_spotcny_userinfo") {
          if (j[0].count("errorcode") == 1) {
            string ec = j[0]["errorcode"];
            log->output("COULDN'T FETCH USER INFO WITH ERROR: " + error_reasons[ec]);
          }
          else {
            if (userinfo_callback) {
              auto funds = j[0]["data"]["info"]["funds"];
              UserInfo info;
              info.asset_net = optionally_to_double(funds["asset"]["net"]);
              info.free_btc = optionally_to_double(funds["free"]["btc"]);
              info.free_cny = optionally_to_double(funds["free"]["cny"]);
              if (funds.count("borrow") == 1) {
                info.borrow_btc = optionally_to_double(funds["borrow"]["btc"]);
                info.borrow_cny = optionally_to_double(funds["borrow"]["cny"]);
              }
              else {
                info.borrow_btc = 0;
                info.borrow_cny = 0;
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

void OKCoin::on_fail() {
  log->output("FAIL with error: " + ws.get_error_reason());
  reconnect = true;
}

void OKCoin::on_error(string const & error_message) {
  log->output("ERROR with message: " + error_message);
  reconnect = true;
}

void OKCoin::subscribe_to_ticker() {
  subscribe_to_channel("ok_sub_spotcny_btc_ticker");
}

void OKCoin::subscribe_to_OHLC(minutes period) {
  subscribe_to_channel("ok_sub_spotcny_btc_kline_" + period_conversions(period));
}

void OKCoin::subscribe_to_channel(string const & channel) {
  log->output("SUBSCRIBING TO " + channel);

  ws.send("{'event':'addChannel','channel':'" + channel + "'}");

  shared_ptr<Channel> chan(new Channel(channel, "subscribing"));
  channels[channel] = chan;
}

void OKCoin::market_buy(double cny_amount) {
  order("buy_market", dtos(cny_amount, 2));
}

void OKCoin::market_sell(double btc_amount) {
  order("sell_market", dtos(btc_amount, 4));
}

void OKCoin::limit_buy(double amount, double price) {
  order("buy", dtos(amount, 4), dtos(price, 2));
}

void OKCoin::limit_sell(double amount, double price) {
  order("sell", dtos(amount, 4), dtos(price, 2));
}

void OKCoin::cancel_order(std::string order_id) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_spotcny_cancel_order";

  json p;
  p["api_key"] = api_key;
  p["symbol"] = "btc_cny";
  p["order_id"] = order_id;
  p["sign"] = sign("api_key=" + api_key + "&order_id=" + order_id + "&symbol=btc_cny&secret_key=" + secret_key);

  j["parameters"] = p;

  ws.send(j.dump());
}

void OKCoin::order(string type, string amount, string price) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_spotcny_trade";

  json parameters;
  // okcoin, for buy market orders, specifies amount to buy in CNY using price field
  // and, for sell market orders, specifies amount to sell in CNY using amount
  if (type == "buy_market")
    parameters["price"] = amount;
  else
    parameters["amount"] = amount;
  parameters["api_key"] = api_key;
  parameters["symbol"] = "btc_cny";
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
  str += "&symbol=btc_cny&type=" + type + "&secret_key=" + secret_key;

  parameters["sign"] = sign(str);

  j["parameters"] = parameters;

  ws.send(j.dump());
}

void OKCoin::orderinfo(string order_id) {
  if (order_id == "failed") {
    OrderInfo failed_order;
    orderinfo_callback(failed_order);
  }
  else {
    json j;
    j["event"] = "addChannel";
    j["channel"] = "ok_spotcny_orderinfo";

    json parameters;
    parameters["api_key"] = api_key;
    parameters["symbol"] = "btc_cny";
    parameters["order_id"] = order_id;
    parameters["sign"] = sign("api_key=" + api_key + "&order_id=" + order_id + "&symbol=btc_cny&secret_key=" + secret_key);

    j["parameters"] = parameters;

    ws.send(j.dump());
  }
}

void OKCoin::userinfo() {
  json j;
  j["event"] = "addChannel";
  j["channel"] = "ok_spotcny_userinfo";

  json p;
  p["api_key"] = api_key;
  p["sign"] = sign("api_key=" + api_key + "&secret_key=" + secret_key);

  j["parameters"] = p;
  ws.send(j.dump());
}

Exchange::BorrowInfo OKCoin::borrow(Currency currency, double amount) {
  double rate = optionally_to_double(lend_depth(currency)[0]["rate"]);
  double can_borrow = optionally_to_double(borrows_info(currency)["can_borrow"]);

  if (amount > can_borrow)
    amount = can_borrow;

  Exchange::BorrowInfo result;
  result.rate = rate;
  result.amount = amount;

  auto response = borrow_money(currency, amount, rate, 15);

  if (response["result"].get<bool>() == true)
    result.id = to_string(response["borrow_id"].get<long>());
  else
    result.id = "failed";

  return result;
}

double OKCoin::close_borrow(Currency currency) {
  // get the open borrows
  auto j = unrepayments_info(currency);
  if (j.size() != 0) {
    string borrow_id = opt_int_to_string(j[0]["borrow_id"]);

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

json OKCoin::lend_depth(Currency currency) {
  string url = "https://www.okcoin.cn/api/v1/lend_depth.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_cny"; break;
    case CNY : post_fields << "&symbol=cny"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j["lend_depth"];
}

json OKCoin::borrows_info(Currency currency) {
  string url = "https://www.okcoin.cn/api/v1/borrows_info.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_cny"; break;
    case CNY : post_fields << "&symbol=cny"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j;
}

json OKCoin::unrepayments_info(Currency currency) {
  string url = "https://www.okcoin.cn/api/v1/unrepayments_info.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  post_fields << "&current_page=1";
  post_fields << "&page_length=10";
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_cny"; break;
    case CNY : post_fields << "&symbol=cny"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j["unrepayments"];
}

json OKCoin::borrow_money(Currency currency, double amount, double rate, int days) {
  string url = "https://www.okcoin.cn/api/v1/borrow_money.do";

  ostringstream post_fields;
  post_fields << "amount=" << amount;
  post_fields << "&api_key=" << api_key;
  if (days == 15)
    post_fields << "&days=fifteen";
  post_fields << "&rate=" << rate;
  switch(currency) {
    case BTC : post_fields << "&symbol=btc_cny"; break;
    case CNY : post_fields << "&symbol=cny"; break;
  }
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j;
}

json OKCoin::repayment(string borrow_id) {
  string url = "https://www.okcoin.cn/api/v1/repayment.do";

  ostringstream post_fields;
  post_fields << "api_key=" << api_key;
  post_fields << "&borrow_id=" << borrow_id;
  string signature = sign(post_fields.str() + "&secret_key=" + secret_key);
  post_fields << "&sign=" << signature;

  string response = curl_post(url, post_fields.str());
  auto j = json::parse(response);
  return j;
}

string OKCoin::sign(string parameters) {
  // generate MD5
  unsigned char result[MD5_DIGEST_LENGTH];
  MD5((unsigned char*) parameters.c_str(), parameters.size(), result);
  ostringstream sout;
  sout << std::hex << std::setfill('0');
  for(long long c: result)
  {
    sout << std::setw(2) << (long long) c;
  }
  string signature = sout.str();
  // convert signature to upper case
  for (auto & c: signature) c = toupper(c);

  return signature;
}

void OKCoin::unsubscribe_to_channel(string const & channel) {
  log->output("UNSUBSCRIBING TO " + channel);
  ws.send("{'event':'removeChannel', 'channel':'" + channel + "'}");
  channels[channel]->status = "unsubscribed";
}

void OKCoin::ping() {
  ws.send("{'event':'ping'}");
}

string OKCoin::status() {
  ostringstream ss;
  for (auto chan : channels) {
    auto c = chan.second;
    ss << c->name << " (" << c->status << "): " << c->last_message << endl;
  }
  return ss.str();
}

void OKCoin::OHLC_handler(string period, json trade, bool backfilling) {
  if (OHLC_callback) {
    long timestamp = optionally_to_long(trade[0]);
    double open = optionally_to_double(trade[1]);
    double high = optionally_to_double(trade[2]);
    double low = optionally_to_double(trade[3]);
    double close = optionally_to_double(trade[4]);
    double volume = optionally_to_double(trade[5]);
    OHLC_callback(period_conversions(period), timestamp, open, high, low, close, volume, backfilling);
  }
}

void OKCoin::ticker_handler(json tick) {
  if (ticker_callback) {
    long timestamp = optionally_to_long(tick["timestamp"]);
    double last = optionally_to_double(tick["last"]);
    double bid = optionally_to_double(tick["buy"]);
    double ask = optionally_to_double(tick["sell"]);

    ticker_callback(timestamp, last, bid, ask);
  }
}

void OKCoin::orderinfo_handler(json order) {
  static map<int, string> statuses = {{-1, "cancelled"},
    {0, "unfilled"}, {1, "partially filled"},
    {2, "fully filled"}, {4, "cancel request in process"}};

  if (orderinfo_callback) {
    double amount = optionally_to_double(order["amount"]);
    double avg_price = optionally_to_double(order["avg_price"]);
    long create_date = optionally_to_long(order["create_date"]);
    double filled_amount = optionally_to_double(order["deal_amount"]);
    int order_id = optionally_to_int(order["order_id"]);
    double price = optionally_to_double(order["price"]);
    string status = statuses[optionally_to_int(order["status"])];
    string symbol = order["symbol"];
    string type = order["type"];

    OrderInfo order(amount, avg_price, create_date,
        filled_amount, order_id, price, status, symbol, type);

    orderinfo_callback(order);
  }
}

void OKCoin::populate_error_reasons() {
  ifstream f("okcoin_error_reasons.txt");
  if (f) {
    // for each line
    string s;
    while (getline(f, s)) {
      // extract key and value, separated by a space
      auto space_location = s.find(' ', 0);
      string key = s.substr(0, space_location);
      string value = s.substr(space_location + 1);
      error_reasons[key] = value;
    }
  }
  else {
    log->output("no error reasons file given");
  }
}

void OKCoin::backfill_OHLC(minutes period, int n) {
  ostringstream url;
  url << "https://www.okcoin.cn/api/v1/kline.do?";
  url << "symbol=btc_cny";
  url << "&type=" << period_conversions(period);
  url << "&size=" << n;

  auto j = json::parse(curl_post(url.str()));

  for (auto i = j.begin(); i != j.end(); ++i) {
    // on the last bar, pretend this isn't backfilling
    bool backfill = next(i) == j.end() ? false : true;
    OHLC_handler(period_conversions(period), *i, backfill);
  }
}
