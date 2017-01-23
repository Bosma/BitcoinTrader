#include "../include/okcoin_futs.h"

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

OKCoinFuts::OKCoinFuts(std::string name, ContractType contract_type, shared_ptr<Log> log, shared_ptr<Config> config) :
  OKCoin(name, Future, log, config),
  contract_type(contract_type) { }

void OKCoinFuts::subscribe_to_ticker() {
  subscribe_to_channel("ok_sub_futureusd_btc_ticker_" + contract_s(contract_type));
}

void OKCoinFuts::subscribe_to_OHLC(minutes period) {
  subscribe_to_channel("ok_sub_futureusd_btc_kline_" + contract_s(contract_type) + "_" + period_s(period));
}

void OKCoinFuts::open(Position position, double amount, double price, double leverage) {
  OrderType order_type = (position == Long) ? OpenLong : OpenShort;
  order(order_type, amount, price, leverage, false);
}

void OKCoinFuts::close(Position position, double amount, double price, double leverage) {
  OrderType order_type = (position == Long) ? CloseLong : CloseShort;
  order(order_type, amount, price, leverage, false);
}

void OKCoinFuts::order(OrderType type, double amount, double price, double lever_rate, bool match_price) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_futureusd_trade";

  json p;
  p["amount"] = dtos(amount, 2);
  p["api_key"] = api_key;
  p["contract_type"] = contract_s(contract_type);
  p["price"] = dtos(price, 2);
  p["symbol"] = "btc_usd";
  p["type"] = dtos(type, 0);
  p["lever_rate"] = dtos(lever_rate, 0);
  p["match_price"] = match_price ? "1" : "0";
  p["sign"] = sign(p);

  j["parameters"] = p;

  ws.send(j.dump());
}

void OKCoinFuts::cancel_order(std::string order_id) {
  json j;

  j["event"] = "addChannel";
  j["channel"] = "ok_futureusd_cancel_order";

  json p;
  p["api_key"] = api_key;
  p["contract_type"] = contract_s(contract_type);
  p["order_id"] = order_id;
  p["symbol"] = "btc_usd";
  p["sign"] = sign(p);

  j["parameters"] = p;

  ws.send(j.dump());
}

void OKCoinFuts::orderinfo(string order_id) {
  if (order_id == "failed") {
    OrderInfo failed_order;
    orderinfo_callback(failed_order);
  }
  else {
    json j;
    j["event"] = "addChannel";
    j["channel"] = "ok_futureusd_orderinfo";

    json p;
    p["api_key"] = api_key;
    p["symbol"] = "btc_usd";
    p["order_id"] = order_id;
    p["contract_type"] = contract_s(contract_type);
    p["current_page"] = "1";
    p["page_length"] = "1";
    p["sign"] = sign(p);

    j["parameters"] = p;

    ws.send(j.dump());
  }
}

void OKCoinFuts::orderinfo_handler(json order) {
  if (orderinfo_callback) {
    OrderInfo new_order;
    new_order.amount = optionally_to_double(order["amount"]);
    new_order.contract_name = order["contract_name"];
    new_order.create_date = opt_to_string<long>(order["create_date"]);
    new_order.filled_amount = optionally_to_double(order["deal_amount"]);
    new_order.fee = optionally_to_double(order["fee"]);
    new_order.lever_rate = optionally_to_int(order["lever_rate"]);
    new_order.order_id = opt_to_string<long>(order["order_id"]);
    new_order.price = optionally_to_double(order["price"]);
    new_order.avg_price = optionally_to_double(order["price_avg"]);
    new_order.status = status_s.at(optionally_to_int(order["status"]));
    new_order.symbol = order["symbol"];
    new_order.type = static_cast<OrderType>(optionally_to_int(order["type"]));
    new_order.unit_amount = optionally_to_int(order["unit_amount"]);

    orderinfo_callback(new_order);
  }
}

void OKCoinFuts::userinfo_handler(nlohmann::json j) {
  if (userinfo_callback) {
    auto data = j["info"]["btc"];
    if (data.count("contracts") == 0) {
      UserInfo info;
      info.equity = optionally_to_double(data["account_rights"]);
      info.margin = optionally_to_double(data["keep_deposit"]);
      info.realized = optionally_to_double(data["profit_real"]);
      info.unrealized = optionally_to_double(data["profit_unreal"]);
      userinfo_callback(info);
    }
    else
      log->output("MUST BE IN CROSS MARGIN MODE");
  }
}
