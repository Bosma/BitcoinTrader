#include "exchanges/okcoin_futs.h"

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
using std::next;         using boost::optional;

OKCoinFuts::OKCoinFuts(std::string name, ContractType contract_type, shared_ptr<Log> log, shared_ptr<Config> config) :
    OKCoin(name, Market::Future, log, config),
    contract_type(contract_type) { }

void OKCoinFuts::subscribe_to_ticker() {
  subscribe_to_channel("ok_sub_futureusd_btc_ticker_" + contract_s(contract_type));
}

void OKCoinFuts::subscribe_to_OHLC(minutes period) {
  subscribe_to_channel("ok_sub_futureusd_btc_kline_" + contract_s(contract_type) + "_" + period_s(period));
}

void OKCoinFuts::subscribe_to_depth() {
  subscribe_to_channel("ok_sub_future_btc_depth_" + contract_s(contract_type) + "_usd");
}

bool OKCoinFuts::subscribed_to_OHLC(minutes period) {
  string channel = "ok_sub_futureusd_btc_kline_" + contract_s(contract_type) + "_" + period_s(period);

  lock_guard<mutex> channels_lock;
  return channels.count(channel) == 1;
}

void OKCoinFuts::open(Position position, double amount, double price, int leverage, timestamp_t invalid_time) {
  OrderType order_type = (position == Position::Long) ? OrderType::OpenLong : OrderType::OpenShort;
  order(order_type, amount, price, leverage, false, invalid_time);
}

void OKCoinFuts::close(Position position, double amount, double price, int leverage, timestamp_t invalid_time) {
  OrderType order_type = (position == Position::Long) ? OrderType::CloseLong : OrderType::CloseShort;
  order(order_type, amount, price, leverage, false, invalid_time);
}

void OKCoinFuts::order(OrderType type, double amount, double price, int lever_rate, bool match_price, timestamp_t invalid_time) {
  string channel = "ok_futureusd_trade";

  {
    lock_guard<mutex> l(channel_timeouts_lock);
    channel_timeouts[channel] = invalid_time;
  }

  json j;

  j["event"] = "addChannel";
  j["channel"] = channel;

  json p;
  p["amount"] = dtos(amount, 2);
  p["api_key"] = api_key;
  p["contract_type"] = contract_s(contract_type);
  p["price"] = dtos(price, 2);
  p["symbol"] = "btc_usd";
  p["type"] = dtos(static_cast<int>(type), 0);
  p["lever_rate"] = dtos(lever_rate, 0);
  p["match_price"] = match_price ? "1" : "0";
  string sig = sign(p);
  p["sign"] = sig;

  j["parameters"] = p;

  ws.send(j.dump());
}

void OKCoinFuts::cancel_order(const std::string& order_id, timestamp_t invalid_time) {
  string channel = "ok_futureusd_cancel_order";

  {
    lock_guard<mutex> l(channel_timeouts_lock);
    channel_timeouts[channel] = invalid_time;
  }

  json j;

  j["event"] = "addChannel";
  j["channel"] = channel;

  json p;
  p["api_key"] = api_key;
  p["contract_type"] = contract_s(contract_type);
  p["order_id"] = order_id;
  p["symbol"] = "btc_usd";
  string sig = sign(p);
  p["sign"] = sig;

  j["parameters"] = p;

  ws.send(j.dump());
}

void OKCoinFuts::orderinfo(const string& order_id, timestamp_t invalid_time) {
  string channel = "ok_futureusd_orderinfo";

  {
    lock_guard<mutex> l(channel_timeouts_lock);
    channel_timeouts[channel] = invalid_time;
  }

  if (order_id == "failed") {
    OrderInfo failed_order;
    failed_order.status = OrderStatus::Failed;
    orderinfo_callback(failed_order);
  }
  else {
    json j;
    j["event"] = "addChannel";
    j["channel"] = channel;

    json p;
    p["api_key"] = api_key;
    p["symbol"] = "btc_usd";
    p["order_id"] = order_id;
    p["contract_type"] = contract_s(contract_type);
    p["current_page"] = "1";
    p["page_length"] = "1";
    string sig = sign(p);
    p["sign"] = sig;

    j["parameters"] = p;

    ws.send(j.dump());
  }
}

optional<OKCoinFuts::FuturePosition> OKCoinFuts::positions() {
  string url = "https://www.okcoin.com/api/v1/future_position.do";

  json p;
  p["api_key"] = api_key;
  p["contract_type"] = contract_s(contract_type);
  p["symbol"] = "btc_usd";
  string sig = sign(p);
  p["sign"] = sig;

  string response = curl_post(url, log, ampersand_list(p));
  if (!response.empty() && response.at(0) != '<') {
    try {
      json j = json::parse(response);
      if (j.count("errorcode") == 1) {
        log->output("COULDN'T FETCH FUTUREPOSITION WITH ERROR: " + error_reasons[j["errorcode"]]);
      }
      else {
        FuturePosition pos{};
        if (j["holding"] != nullptr && !j["holding"].empty()) {
          const json& h = j["holding"][0];
          pos.buy.contracts = optionally_to<int>(h["buy_amount"]);
          pos.buy.contracts_can_close = optionally_to<int>(h["buy_available"]);
          pos.buy.avg_open_price = optionally_to<double>(h["buy_price_avg"]);
          pos.buy.cost_price = optionally_to<double>(h["buy_price_cost"]);
          pos.buy.realized_profit = optionally_to<double>(h["buy_profit_real"]);
          pos.sell.contracts = optionally_to<int>(h["sell_amount"]);
          pos.sell.contracts_can_close = optionally_to<int>(h["sell_available"]);
          pos.sell.avg_open_price = optionally_to<double>(h["sell_price_avg"]);
          pos.sell.cost_price = optionally_to<double>(h["sell_price_cost"]);
          pos.sell.realized_profit = optionally_to<double>(h["sell_profit_real"]);
          pos.contract_id = opt_to_string<long>(h["contract_id"]);
          pos.create_date = opt_to_string<long>(h["create_date"]);
          pos.lever_rate = optionally_to<int>(h["lever_rate"]);
        }
        return pos;
      }
    }
    catch (exception &e) {
      log->output("OKCoinFuts::positions(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
    }
  }
  return {};
}

bool OKCoinFuts::backfill_OHLC(minutes period, unsigned long n) {
  string url = "https://www.okcoin.com/api/v1/future_kline.do?symbol=btc_usd&contract_type=" + contract_s(contract_type) +
      "&type=" + period_s(period) + "&size=" + to_string(n);

  auto response = curl_post(url, log);
  if (!response.empty() && response.at(0) != '<') {
    try {
      json j = json::parse(response);
      for (auto& each : j)
        OHLC_handler(period_s(period), each);
      return true;
    }
    catch (exception &e) {
      log->output("OKCoinFuts::backfill_OHLC(): ERROR PARSING JSON " + string(e.what()) + ", with: " + response);
      return false;
    }
  }
  else
    return false;
}

void OKCoinFuts::orderinfo_handler(const json& order) {
  if (orderinfo_callback) {
    OrderInfo new_order;
    new_order.amount = optionally_to<double>(order["amount"]);
    new_order.contract_name = order["contract_name"];
    new_order.create_date = opt_to_string<long>(order["create_date"]);
    new_order.filled_amount = optionally_to<double>(order["deal_amount"]);
    new_order.fee = optionally_to<double>(order["fee"]);
    new_order.lever_rate = optionally_to<int>(order["lever_rate"]);
    new_order.order_id = opt_to_string<long>(order["order_id"]);
    new_order.price = optionally_to<double>(order["price"]);
    new_order.avg_price = optionally_to<double>(order["price_avg"]);
    new_order.status = static_cast<OrderStatus>(optionally_to<int>(order["status"]));
    new_order.symbol = order["symbol"];
    new_order.type = static_cast<OrderType>(optionally_to<int>(order["type"]));
    new_order.unit_amount = optionally_to<int>(order["unit_amount"]);
    new_order.timestamp = timestamp_now();
    orderinfo_callback(new_order);
  }
}

void OKCoinFuts::userinfo_handler(const json& j) {
  if (userinfo_callback) {
    const json& data = j["info"]["btc"];
    if (data.count("contracts") == 0) {
      UserInfo info;
      info.equity = optionally_to<double>(data["account_rights"]);
      info.margin = optionally_to<double>(data["keep_deposit"]);
      info.realized = optionally_to<double>(data["profit_real"]);
      info.unrealized = optionally_to<double>(data["profit_unreal"]);
      userinfo_callback(info);
    }
    else
      log->output("MUST BE IN CROSS MARGIN MODE");
  }
}