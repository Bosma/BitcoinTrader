#include "exchanges/okcoin.h"

using std::shared_ptr;
using std::bind;
using std::ifstream;
using std::string;
using std::ostringstream;
using std::endl;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::nanoseconds;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::sort;
using std::accumulate;
using std::next;
using std::exception;
using std::stringstream;
using std::to_string;
using std::sort;

OKCoin::OKCoin(string name, Market market, shared_ptr<Log> log, shared_ptr<Config> config) :
    Exchange(name, log, config),
    api_key((*config)["okcoin_apikey"]),
    secret_key((*config)["okcoin_secretkey"]),
    ws(OKCOIN_URL),
    market(market),
    error_reasons() {
  ws.set_open_callback( bind(&OKCoin::on_open, this) );
  ws.set_message_callback( bind(&OKCoin::on_message, this, std::placeholders::_1) );
  ws.set_close_callback( bind(&OKCoin::on_close, this) );
  ws.set_fail_callback( bind(&OKCoin::on_fail, this) );
  ws.set_error_callback( bind(&OKCoin::on_error, this, std::placeholders::_1) );

  populate_error_reasons();
}

void OKCoin::start() {
  ws.connect();
}

void OKCoin::on_message(const string& message) {
  ts_since_last.set(timestamp_now());

  try {
    const json j = json::parse(message);

    // if our json is an array
    // it's a response from some channel
    if (j.is_array()) {
      const json& channel_message = j[0];
      // there should be a channel key
      if (channel_message.count("channel") == 1) {
        // fetch the channel name
        const string& channel = channel_message["channel"];
        const json& data = channel_message["data"];

        // store the time of the last message received
        if (channels.count(channel) == 1)
          channels.at(channel).last_message_time = timestamp_now();

        // if we're adding a channel
        if (channel == "addChannel") {
          // we're subscribing to a channel
          const string &channel_name = data["channel"];
          if (data["result"]) {
            log->output("SUBSCRIBED TO " + channel_name);
            channels.emplace(channel_name, channel_name);
          }
          else
            log->output("UNSUCCESSFULLY SUBSCRIBED TO " + channel_name);
        }
        // or if it's a channel we have a handler for
        else if (channel.find("ok_sub_" + market_s(market) + "usd_btc_ticker") != string::npos) {
          ticker_handler(data);
        }
        else if (channel.find("ok_sub_" + market_s(market) + "_btc_depth") != string::npos) {
          depth_handler(data);
        }
        else if (channel.find("ok_sub_" + market_s(market) + "usd_btc_kline_") != string::npos) {
          // remove beginning of channel name to obtain period
          std::regex period_r("_([^_]*)$", std::regex::extended);
          std::smatch match;
          string period;
          if (std::regex_search(channel, match, period_r))
            period = match[1];
          else
            throw std::runtime_error("cannot get the period using regex");

          if (data[0].is_array()) // data is an array of trades
            for (auto& trade : data)
              OHLC_handler(period, trade);
          else // data is a trade
            OHLC_handler(period, data);
        }
        // check for one off channel messages that have callbacks
        // these are grouped together because they have an expiry
        else {
          // only process messages that have no timeout or their timeout hasn't been reached
          auto ts = timestamp_now();
          if (channel_timeouts.count(channel) == 0 ||
              ts <= channel_timeouts[channel]) {
            if (channel == "ok_" + market_s(market) + "usd_trade") {
              // results of a trade
              if (channel_message.count("errorcode") == 1) {
                log->output("FAILED TRADE ON " + channel + " WITH ERROR: " + error_reasons[channel_message["errorcode"].get<string>()]);
                if (trade_callback)
                  trade_callback("failed");
              }
              else {
                const string& order_id = opt_to_string<long>(data["order_id"]);
                if (data["result"]) {
                  if (trade_callback)
                    trade_callback(order_id);
                }
                else {
                  log->output("FAILED TRADE (" + order_id + ") ON " + channel);
                  if (trade_callback)
                    trade_callback("failed");
                }
              }
            }
            else if (channel == "ok_" + market_s(market) + "usd_orderinfo") {
              if (data["orders"].empty())
                log->output(channel + " MESSAGE RECEIVED BY INVALID ORDER ID");
              else
                orderinfo_handler(data["orders"][0]);
            }
            else if (channel == "ok_" + market_s(market) + "usd_userinfo") {
              if (channel_message.count("errorcode") == 1)
                log->output("COULDN'T FETCH USER INFO WITH ERROR: " + error_reasons[channel_message["errorcode"]]);
              else
                userinfo_handler(channel_message["data"]);
            }
            else if (channel == "ok_" + market_s(market) + "usd_cancel_order") {
              if (j[0].count("errorcode") == 1)
                log->output("COULDN'T CANCEL ORDER " + channel_message["order_id"].get<string>() + " WITH ERROR: " +
                            error_reasons[channel_message["errorcode"]]);
            }
            else if (channel == "btc_forecast_price") {
              // no implementation - spammed an hour before contract close even if we are not subscribed
            }
            else if (channel == "ok_btc_future_contract") {
              // no implementation - sent right as contract closed
            }
            else {
              log->output("MESSAGE WITH UNKNOWN CHANNEL, JSON: " + message);
            }

            // erase the timeout
            if (channel_timeouts.count(channel) == 1)
              channel_timeouts.erase(channel);
          }
          // channel timeout has been reached
          else {
            log->output("CHANNEL " + channel + " MESSAGE RECEIVED BUT TIMEOUT REACHED, NOT CALLING CALLBACK");
          }
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
  catch (const exception& e) {
    log->output("EXCEPTION IN OKCoin::on_message:" + string(e.what()) + " WITH JSON: " + message);
  }
}

void OKCoin::on_open() {
  log->output("OPENED SOCKET to " + ws.get_uri());

  if (open_callback)
    open_callback();
}

void OKCoin::on_close() {
  log->output("CLOSE with reason: " + ws.get_error_reason());

  // 1001 is normal close
  if (ws.get_close_code() != 1001)
    log->output("WS ABNORMAL CLOSE CODE");
}

void OKCoin::on_fail() {
  log->output("FAIL with error: " + ws.get_error_reason());
}

void OKCoin::on_error(string const & error_message) {
  log->output("ERROR with message: " + error_message);
}

void OKCoin::ping() {
  ws.send("{'event':'ping'}");
}

string OKCoin::status() {
  string ss = name + ": " + ws.get_status_s() + "\n";
  for (auto& chan : channels)
    ss += chan.second.to_string() + "\n";
  return ss;
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

void OKCoin::userinfo(timestamp_t invalid_time) {
  string channel = "ok_" + market_s(market) + "usd_userinfo";

  channel_timeouts[channel] = invalid_time;

  json j;
  j["event"] = "addChannel";
  j["channel"] = channel;

  json p;
  p["api_key"] = api_key;
  string sig = sign(p);
  p["sign"] = sig;

  j["parameters"] = p;
  ws.send(j.dump());
}

void OKCoin::subscribe_to_channel(string const & channel) {
  log->output("SUBSCRIBING TO " + channel);

  ws.send("{'event':'addChannel','channel':'" + channel + "'}");
}

void OKCoin::unsubscribe_to_channel(string const & channel) {
  if (channels.count(channel)) {
    log->output("UNSUBSCRIBING TO " + channel);
    ws.send("{'event':'removeChannel', 'channel':'" + channel + "'}");
    channels.erase(channel);
  }
  else
    log->output("ATTEMPT TO UNSUBSCRIBE TO CHANNEL " + channel + " BUT NOT SUBSCRIBED");
}

void OKCoin::OHLC_handler(const string& period, const json& trade) {
  if (OHLC_callback) {
    timestamp_t timestamp(duration_cast<nanoseconds>(milliseconds(optionally_to<long>(trade[0]))));
    double open = optionally_to<double>(trade[1]);
    double high = optionally_to<double>(trade[2]);
    double low = optionally_to<double>(trade[3]);
    double close = optionally_to<double>(trade[4]);
    double volume = optionally_to<double>(trade[5]);
    OHLC new_bar(timestamp, open, high, low, close, volume);
    OHLC_callback(period_m(period), new_bar);
  }
}

void OKCoin::ticker_handler(const json& j) {
  if (ticker_callback) {
    double last = optionally_to<double>(j["last"]);
    double bid = optionally_to<double>(j["buy"]);
    double ask = optionally_to<double>(j["sell"]);
    Ticker tick(last, bid, ask, timestamp_now());

    ticker_callback(tick);
  }
}

void OKCoin::depth_handler(const json& j) {
  if (depth_callback) {
    Depth depth;

    for (const json& order: j["asks"]) {
      depth.asks.emplace_back(optionally_to<double>(order[0]),
                              optionally_to<double>(order[1]));
    }
    // ensure asks are sorted normally
    sort(depth.asks.begin(), depth.asks.end());

    for (const json& order : j["bids"]) {
      depth.bids.emplace_back(optionally_to<double>(order[0]),
                              optionally_to<double>(order[1]));
    }
    // ensure bids are sorted in reverse
    sort(depth.bids.begin(), depth.bids.end(),
         [](Depth::Order a, Depth::Order b) { return b < a; });

    timestamp_t t(duration_cast<nanoseconds>(milliseconds(optionally_to<long>(j["timestamp"]))));
    depth.timestamp = t;
    depth_callback(depth);
  }
}

string OKCoin::ampersand_list(const json& j) {
  // convert to map (which is sorted by default)
  std::map<string, string> parameters;
  for (auto it = j.begin(); it != j.end(); ++it)
    parameters[it.key()] = it.value().get<string>();

  // join them by &=
  auto how_to_join = [](std::string a, std::pair<std::string, std::string> b) {
    return a + "&" + b.first + "=" + b.second;
  };

  std::string splatted = std::accumulate(
      next(parameters.begin()),
      parameters.end(),
      parameters.begin()->first + "=" + parameters.begin()->second,
      how_to_join);
  return splatted;
};

string OKCoin::sign(const json& parameters) {
  string to_sign = ampersand_list(parameters);
  to_sign += "&secret_key=" + secret_key;
  return get_sig(to_sign);
}

string OKCoin::get_sig(const string& s) {
  unsigned char result[MD5_DIGEST_LENGTH];
  MD5((unsigned char*) s.c_str(), s.size(), result);
  ostringstream sout;
  sout << std::hex << std::setfill('0');
  for(long long c: result)
  {
    sout << std::setw(2) << (long long) c;
  }
  string signature = sout.str();
  // convert signature to upper case
  for (auto & c: signature) c = static_cast<char>(toupper(c));

  return signature;
}
